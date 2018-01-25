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
  if (m_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_render_pass, nullptr);
  TextureCache::DeleteShaders();
}

VkShaderModule TextureCache::GetCopyShader() const
{
  return m_copy_shader;
}

VkRenderPass TextureCache::GetTextureCopyRenderPass() const
{
  return m_render_pass;
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

  if (!CreateRenderPasses())
  {
    PanicAlert("Failed to create copy render pass");
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
  m_texture_converter->ConvertTexture(destination, source, m_render_pass, palette, format);

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

void TextureCache::CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width,
                           u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                           const EFBRectangle& src_rect, bool scale_by_half)
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
  StateTracker::GetInstance()->OnReadback();

  // Transition to shader resource before reading.
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_texture_converter->EncodeTextureToMemory(src_texture->GetView(), dst, params, native_width,
                                             bytes_per_row, num_blocks_y, memory_stride, src_rect,
                                             scale_by_half);

  // Transition back to original state
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), original_layout);
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

std::unique_ptr<AbstractTexture> TextureCache::CreateTexture(const TextureConfig& config)
{
  return VKTexture::Create(config);
}

bool TextureCache::CreateRenderPasses()
{
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif
  static constexpr VkAttachmentDescription update_attachment = {
      0,
      TEXTURECACHE_TEXTURE_FORMAT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  static constexpr VkAttachmentReference color_attachment_reference = {
      0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  static constexpr VkSubpassDescription subpass_description = {
      0,       VK_PIPELINE_BIND_POINT_GRAPHICS,
      0,       nullptr,
      1,       &color_attachment_reference,
      nullptr, nullptr,
      0,       nullptr};

  VkRenderPassCreateInfo update_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        nullptr,
                                        0,
                                        1,
                                        &update_attachment,
                                        1,
                                        &subpass_description,
                                        0,
                                        nullptr};

  VkResult res =
      vkCreateRenderPass(g_vulkan_context->GetDevice(), &update_info, nullptr, &m_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  return true;
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

  static const char EFB_COLOR_TO_TEX_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;

    layout(std140, push_constant) uniform PSBlock
    {
      vec4 colmat[7];
    } C;

    layout(location = 0) in vec3 uv0;
    layout(location = 1) in vec4 col0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      float4 texcol = texture(samp0, uv0);
      texcol = floor(texcol * C.colmat[5]) * C.colmat[6];
      ocol0 = texcol * mat4(C.colmat[0], C.colmat[1], C.colmat[2], C.colmat[3]) + C.colmat[4];
    }
  )";

  static const char EFB_DEPTH_TO_TEX_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;

    layout(std140, push_constant) uniform PSBlock
    {
      vec4 colmat[5];
    } C;

    layout(location = 0) in vec3 uv0;
    layout(location = 1) in vec4 col0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      #if MONO_DEPTH
        vec4 texcol = texture(samp0, vec3(uv0.xy, 0.0f));
      #else
        vec4 texcol = texture(samp0, uv0);
      #endif
      int depth = int((1.0 - texcol.x) * 16777216.0);

      // Convert to Z24 format
      ivec4 workspace;
      workspace.r = (depth >> 16) & 255;
      workspace.g = (depth >> 8) & 255;
      workspace.b = depth & 255;

      // Convert to Z4 format
      workspace.a = (depth >> 16) & 0xF0;

      // Normalize components to [0.0..1.0]
      texcol = vec4(workspace) / 255.0;

      ocol0 = texcol * mat4(C.colmat[0], C.colmat[1], C.colmat[2], C.colmat[3]) + C.colmat[4];
    }
  )";

  std::string header = g_shader_cache->GetUtilityShaderHeader();
  std::string source;

  source = header + COPY_SHADER_SOURCE;
  m_copy_shader = Util::CompileAndCreateFragmentShader(source);

  source = header + EFB_COLOR_TO_TEX_SOURCE;
  m_efb_color_to_tex_shader = Util::CompileAndCreateFragmentShader(source);

  if (g_ActiveConfig.bStereoEFBMonoDepth)
    source = header + "#define MONO_DEPTH 1\n" + EFB_DEPTH_TO_TEX_SOURCE;
  else
    source = header + EFB_DEPTH_TO_TEX_SOURCE;
  m_efb_depth_to_tex_shader = Util::CompileAndCreateFragmentShader(source);

  return m_copy_shader != VK_NULL_HANDLE && m_efb_color_to_tex_shader != VK_NULL_HANDLE &&
         m_efb_depth_to_tex_shader != VK_NULL_HANDLE;
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
  if (m_efb_color_to_tex_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_efb_color_to_tex_shader, nullptr);
    m_efb_color_to_tex_shader = VK_NULL_HANDLE;
  }
  if (m_efb_depth_to_tex_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_efb_depth_to_tex_shader, nullptr);
    m_efb_depth_to_tex_shader = VK_NULL_HANDLE;
  }
}

void TextureCache::CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                       const EFBRectangle& src_rect, bool scale_by_half,
                                       unsigned int cbuf_id, const float* colmat)
{
  VKTexture* texture = static_cast<VKTexture*>(entry->texture.get());

  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);

  // Flush EFB pokes first, as they're expected to be included.
  framebuffer_mgr->FlushEFBPokes();

  // Has to be flagged as a render target.
  _assert_(texture->GetFramebuffer() != VK_NULL_HANDLE);

  // Can't be done in a render pass, since we're doing our own render pass!
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  StateTracker::GetInstance()->EndRenderPass();

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

  UtilityShaderDraw draw(command_buffer,
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_PUSH_CONSTANT),
                         m_render_pass, g_shader_cache->GetPassthroughVertexShader(),
                         g_shader_cache->GetPassthroughGeometryShader(),
                         is_depth_copy ? m_efb_depth_to_tex_shader : m_efb_color_to_tex_shader);

  draw.SetPushConstants(colmat, (is_depth_copy ? sizeof(float) * 20 : sizeof(float) * 28));
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
