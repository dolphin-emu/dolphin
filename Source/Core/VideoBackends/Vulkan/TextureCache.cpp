// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/TextureCache.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureConverter.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConfig.h"

namespace Vulkan
{
TextureCache::TextureCache()
{
}

TextureCache::~TextureCache()
{
  TextureCache::DeleteShaders();
}

VkShaderModule TextureCache::GetCopyShader() const
{
  return m_copy_shader;
}

StreamBuffer* TextureCache::GetTextureUploadBuffer() const
{
  return m_texture_upload_buffer.get();
}

TextureCache* TextureCache::GetInstance()
{
  return static_cast<TextureCache*>(g_texture_cache.get());
}

bool TextureCache::Initialize()
{
  m_texture_upload_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE,
                           MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE);
  if (!m_texture_upload_buffer)
  {
    PanicAlert("Failed to create texture upload buffer");
    return false;
  }

  m_texture_converter = std::make_unique<TextureConverter>();
  if (!m_texture_converter->Initialize())
  {
    PanicAlert("Failed to initialize texture converter");
    return false;
  }

  if (!CompileShaders())
  {
    PanicAlert("Failed to compile one or more shaders");
    return false;
  }

  return true;
}

void TextureCache::ConvertTexture(TCacheEntry* destination, TCacheEntry* source,
                                  const void* palette, TLUTFormat format)
{
  m_texture_converter->ConvertTexture(destination, source, palette, format);

  // Ensure both textures remain in the SHADER_READ_ONLY layout so they can be bound.
  static_cast<VKTexture*>(source->texture.get())
      ->GetRawTexIdentifier()
      ->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  static_cast<VKTexture*>(destination->texture.get())
      ->GetRawTexIdentifier()
      ->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params,
                           u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                           const EFBRectangle& src_rect, bool scale_by_half, float y_scale,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const CopyFilterCoefficientArray& filter_coefficients)
{
  // Flush EFB pokes first, as they're expected to be included.
  FramebufferManager::GetInstance()->FlushEFBPokes();

  // MSAA case where we need to resolve first.
  // An out-of-bounds source region is valid here, and fine for the draw (since it is converted
  // to texture coordinates), but it's not valid to resolve an out-of-range rectangle.
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
  VkRect2D region = {{scaled_src_rect.left, scaled_src_rect.top},
                     {static_cast<u32>(scaled_src_rect.GetWidth()),
                      static_cast<u32>(scaled_src_rect.GetHeight())}};
  region = Util::ClampRect2D(region, FramebufferManager::GetInstance()->GetEFBWidth(),
                             FramebufferManager::GetInstance()->GetEFBHeight());
  Texture2D* src_texture;
  if (params.depth)
    src_texture = FramebufferManager::GetInstance()->ResolveEFBDepthTexture(region);
  else
    src_texture = FramebufferManager::GetInstance()->ResolveEFBColorTexture(region);

  // End render pass before barrier (since we have no self-dependencies).
  // The barrier has to happen after the render pass, not inside it, as we are going to be
  // reading from the texture immediately afterwards.
  StateTracker::GetInstance()->EndRenderPass();

  // Transition to shader resource before reading.
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_texture_converter->EncodeTextureToMemory(
      src_texture->GetView(), dst, params, native_width, bytes_per_row, num_blocks_y, memory_stride,
      src_rect, scale_by_half, y_scale, gamma, clamp_top, clamp_bottom, filter_coefficients);

  // Transition back to original state
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), original_layout);

  StateTracker::GetInstance()->OnEFBCopyToRAM();
}

bool TextureCache::SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format)
{
  return m_texture_converter->SupportsTextureDecoding(format, palette_format);
}

void TextureCache::DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data,
                                      size_t data_size, TextureFormat format, u32 width, u32 height,
                                      u32 aligned_width, u32 aligned_height, u32 row_stride,
                                      const u8* palette, TLUTFormat palette_format)
{
  // Group compute shader dispatches together in the init command buffer. That way we don't have to
  // pay a penalty for switching from graphics->compute, or end/restart our render pass.
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentInitCommandBuffer();
  m_texture_converter->DecodeTexture(command_buffer, entry, dst_level, data, data_size, format,
                                     width, height, aligned_width, aligned_height, row_stride,
                                     palette, palette_format);

  // Last mip level? Ensure the texture is ready for use.
  if (dst_level == (entry->GetNumLevels() - 1))
  {
    static_cast<VKTexture*>(entry->texture.get())
        ->GetRawTexIdentifier()
        ->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

bool TextureCache::CompileShaders()
{
  static const char COPY_SHADER_SOURCE[] = R"(
    layout(set = 1, binding = 0) uniform sampler2DArray samp0;

    layout(location = 0) in float3 uv0;
    layout(location = 1) in float4 col0;
    layout(location = 0) out float4 ocol0;

    void main()
    {
      ocol0 = texture(samp0, uv0);
    }
  )";

  std::string header = g_shader_cache->GetUtilityShaderHeader();
  std::string source = header + COPY_SHADER_SOURCE;

  m_copy_shader = Util::CompileAndCreateFragmentShader(source);

  return m_copy_shader != VK_NULL_HANDLE;
}

void TextureCache::DeleteShaders()
{
  // It is safe to destroy shader modules after they are consumed by creating a pipeline.
  // Therefore, no matter where this function is called from, it won't cause an issue due to
  // pending commands, although at the time of writing should only be called at the end of
  // a frame. See Vulkan spec, section 2.3.1. Object Lifetime.
  if (m_copy_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_copy_shader, nullptr);
    m_copy_shader = VK_NULL_HANDLE;
  }

  for (auto& shader : m_efb_copy_to_tex_shaders)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), shader.second, nullptr);
  }
  m_efb_copy_to_tex_shaders.clear();
}

void TextureCache::CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                       const EFBRectangle& src_rect, bool scale_by_half,
                                       EFBCopyFormat dst_format, bool is_intensity, float gamma,
                                       bool clamp_top, bool clamp_bottom,
                                       const CopyFilterCoefficientArray& filter_coefficients)
{
  VKTexture* texture = static_cast<VKTexture*>(entry->texture.get());

  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);

  // Flush EFB pokes first, as they're expected to be included.
  framebuffer_mgr->FlushEFBPokes();

  // Has to be flagged as a render target.
  ASSERT(texture->GetFramebuffer() != VK_NULL_HANDLE);

  // Can't be done in a render pass, since we're doing our own render pass!
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  StateTracker::GetInstance()->EndRenderPass();

  // Fill uniform buffer.
  struct PixelUniforms
  {
    float filter_coefficients[3];
    float gamma_rcp;
    float clamp_top;
    float clamp_bottom;
    float pixel_height;
    u32 padding;
  };
  PixelUniforms uniforms;
  for (size_t i = 0; i < filter_coefficients.size(); i++)
    uniforms.filter_coefficients[i] = filter_coefficients[i];
  uniforms.gamma_rcp = 1.0f / gamma;
  uniforms.clamp_top = clamp_top ? src_rect.top / float(EFB_HEIGHT) : 0.0f;
  uniforms.clamp_bottom = clamp_bottom ? src_rect.bottom / float(EFB_HEIGHT) : 1.0f;
  uniforms.pixel_height =
      g_ActiveConfig.bCopyEFBScaled ? 1.0f / g_renderer->GetTargetHeight() : 1.0f / EFB_HEIGHT;
  uniforms.padding = 0;

  // Transition EFB to shader resource before binding.
  // An out-of-bounds source region is valid here, and fine for the draw (since it is converted
  // to texture coordinates), but it's not valid to resolve an out-of-range rectangle.
  VkRect2D region = {{scaled_src_rect.left, scaled_src_rect.top},
                     {static_cast<u32>(scaled_src_rect.GetWidth()),
                      static_cast<u32>(scaled_src_rect.GetHeight())}};
  region = Util::ClampRect2D(region, FramebufferManager::GetInstance()->GetEFBWidth(),
                             FramebufferManager::GetInstance()->GetEFBHeight());
  Texture2D* src_texture;
  if (is_depth_copy)
    src_texture = FramebufferManager::GetInstance()->ResolveEFBDepthTexture(region);
  else
    src_texture = FramebufferManager::GetInstance()->ResolveEFBColorTexture(region);

  VkSampler src_sampler =
      scale_by_half ? g_object_cache->GetLinearSampler() : g_object_cache->GetPointSampler();
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  texture->GetRawTexIdentifier()->TransitionToLayout(command_buffer,
                                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  auto uid = TextureConversionShaderGen::GetShaderUid(dst_format, is_depth_copy, is_intensity,
                                                      scale_by_half,
                                                      NeedsCopyFilterInShader(filter_coefficients));

  auto it = m_efb_copy_to_tex_shaders.emplace(uid, VkShaderModule(VK_NULL_HANDLE));
  VkShaderModule& shader = it.first->second;
  bool created = it.second;

  if (created)
  {
    std::string source = g_shader_cache->GetUtilityShaderHeader();
    source +=
        TextureConversionShaderGen::GenerateShader(APIType::Vulkan, uid.GetUidData()).GetBuffer();

    shader = Util::CompileAndCreateFragmentShader(source);
  }

  VkRenderPass render_pass = g_object_cache->GetRenderPass(
      texture->GetRawTexIdentifier()->GetFormat(), VK_FORMAT_UNDEFINED,
      texture->GetRawTexIdentifier()->GetSamples(), VK_ATTACHMENT_LOAD_OP_DONT_CARE);

  UtilityShaderDraw draw(command_buffer,
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD), render_pass,
                         g_shader_cache->GetPassthroughVertexShader(),
                         g_shader_cache->GetPassthroughGeometryShader(), shader);

  u8* ubo_ptr = draw.AllocatePSUniforms(sizeof(PixelUniforms));
  std::memcpy(ubo_ptr, &uniforms, sizeof(PixelUniforms));
  draw.CommitPSUniforms(sizeof(PixelUniforms));

  draw.SetPSSampler(0, src_texture->GetView(), src_sampler);

  VkRect2D dest_region = {{0, 0}, {texture->GetConfig().width, texture->GetConfig().height}};

  draw.BeginRenderPass(texture->GetFramebuffer(), dest_region);

  draw.DrawQuad(0, 0, texture->GetConfig().width, texture->GetConfig().height, scaled_src_rect.left,
                scaled_src_rect.top, 0, scaled_src_rect.GetWidth(), scaled_src_rect.GetHeight(),
                framebuffer_mgr->GetEFBWidth(), framebuffer_mgr->GetEFBHeight());

  draw.EndRenderPass();

  // We touched everything, so put it back.
  StateTracker::GetInstance()->SetPendingRebind();

  // Transition the EFB back to its original layout.
  src_texture->TransitionToLayout(command_buffer, original_layout);

  // Ensure texture is in SHADER_READ_ONLY layout, ready for usage.
  texture->GetRawTexIdentifier()->TransitionToLayout(command_buffer,
                                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

}  // namespace Vulkan
