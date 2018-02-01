// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/TextureConverter.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
TextureConverter::TextureConverter()
{
}

TextureConverter::~TextureConverter()
{
  for (const auto& it : m_palette_conversion_shaders)
  {
    if (it != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), it, nullptr);
  }

  if (m_texel_buffer_view_r8_uint != VK_NULL_HANDLE)
    vkDestroyBufferView(g_vulkan_context->GetDevice(), m_texel_buffer_view_r8_uint, nullptr);
  if (m_texel_buffer_view_r16_uint != VK_NULL_HANDLE)
    vkDestroyBufferView(g_vulkan_context->GetDevice(), m_texel_buffer_view_r16_uint, nullptr);
  if (m_texel_buffer_view_r32g32_uint != VK_NULL_HANDLE)
    vkDestroyBufferView(g_vulkan_context->GetDevice(), m_texel_buffer_view_r32g32_uint, nullptr);
  if (m_texel_buffer_view_rgba8_unorm != VK_NULL_HANDLE)
    vkDestroyBufferView(g_vulkan_context->GetDevice(), m_texel_buffer_view_rgba8_unorm, nullptr);

  if (m_encoding_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_encoding_render_pass, nullptr);

  if (m_encoding_render_framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_encoding_render_framebuffer, nullptr);

  for (auto& it : m_encoding_shaders)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), it.second, nullptr);

  for (const auto& it : m_decoding_pipelines)
  {
    if (it.second.compute_shader != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), it.second.compute_shader, nullptr);
  }

  if (m_rgb_to_yuyv_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_rgb_to_yuyv_shader, nullptr);
  if (m_yuyv_to_rgb_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_yuyv_to_rgb_shader, nullptr);
}

bool TextureConverter::Initialize()
{
  if (!CreateTexelBuffer())
  {
    PanicAlert("Failed to create uniform buffer");
    return false;
  }

  if (!CompilePaletteConversionShaders())
  {
    PanicAlert("Failed to compile palette conversion shaders");
    return false;
  }

  if (!CreateEncodingRenderPass())
  {
    PanicAlert("Failed to create encode render pass");
    return false;
  }

  if (!CreateEncodingTexture())
  {
    PanicAlert("Failed to create encoding texture");
    return false;
  }

  if (!CreateEncodingDownloadTexture())
  {
    PanicAlert("Failed to create download texture");
    return false;
  }

  if (!CreateDecodingTexture())
  {
    PanicAlert("Failed to create decoding texture");
    return false;
  }

  if (!CompileYUYVConversionShaders())
  {
    PanicAlert("Failed to compile YUYV conversion shaders");
    return false;
  }

  return true;
}

bool TextureConverter::ReserveTexelBufferStorage(size_t size, size_t alignment)
{
  // Enforce the minimum alignment for texture buffers on the device.
  size_t actual_alignment =
      std::max(static_cast<size_t>(g_vulkan_context->GetTexelBufferAlignment()), alignment);
  if (m_texel_buffer->ReserveMemory(size, actual_alignment))
    return true;

  WARN_LOG(VIDEO, "Executing command list while waiting for space in palette buffer");
  Util::ExecuteCurrentCommandsAndRestoreState(false);

  // This next call should never fail, since a command buffer is now in-flight and we can
  // wait on the fence for the GPU to finish. If this returns false, it's probably because
  // the device has been lost, which is fatal anyway.
  if (!m_texel_buffer->ReserveMemory(size, actual_alignment))
  {
    PanicAlert("Failed to allocate space for texture conversion");
    return false;
  }

  return true;
}

VkCommandBuffer
TextureConverter::GetCommandBufferForTextureConversion(const TextureCache::TCacheEntry* src_entry)
{
  // EFB copies can be used as paletted textures as well. For these, we can't assume them to be
  // contain the correct data before the frame begins (when the init command buffer is executed),
  // so we must convert them at the appropriate time, during the drawing command buffer.
  if (src_entry->IsEfbCopy())
  {
    StateTracker::GetInstance()->EndRenderPass();
    StateTracker::GetInstance()->SetPendingRebind();
    return g_command_buffer_mgr->GetCurrentCommandBuffer();
  }
  else
  {
    // Use initialization command buffer and perform conversion before the drawing commands.
    return g_command_buffer_mgr->GetCurrentInitCommandBuffer();
  }
}

void TextureConverter::ConvertTexture(TextureCacheBase::TCacheEntry* dst_entry,
                                      TextureCacheBase::TCacheEntry* src_entry,
                                      VkRenderPass render_pass, const void* palette,
                                      TLUTFormat palette_format)
{
  struct PSUniformBlock
  {
    float multiplier;
    int texel_buffer_offset;
    int pad[2];
  };

  VKTexture* source_texture = static_cast<VKTexture*>(src_entry->texture.get());
  VKTexture* destination_texture = static_cast<VKTexture*>(dst_entry->texture.get());

  _assert_(static_cast<size_t>(palette_format) < NUM_PALETTE_CONVERSION_SHADERS);
  _assert_(destination_texture->GetConfig().rendertarget);

  // We want to align to 2 bytes (R16) or the device's texel buffer alignment, whichever is greater.
  size_t palette_size = src_entry->format == TextureFormat::I4 ? 32 : 512;
  if (!ReserveTexelBufferStorage(palette_size, sizeof(u16)))
    return;

  // Copy in palette to texel buffer.
  u32 palette_offset = static_cast<u32>(m_texel_buffer->GetCurrentOffset());
  memcpy(m_texel_buffer->GetCurrentHostPointer(), palette, palette_size);
  m_texel_buffer->CommitMemory(palette_size);

  VkCommandBuffer command_buffer = GetCommandBufferForTextureConversion(src_entry);
  source_texture->GetRawTexIdentifier()->TransitionToLayout(
      command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  destination_texture->GetRawTexIdentifier()->TransitionToLayout(
      command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Bind and draw to the destination.
  UtilityShaderDraw draw(command_buffer,
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_TEXTURE_CONVERSION),
                         render_pass, g_shader_cache->GetScreenQuadVertexShader(), VK_NULL_HANDLE,
                         m_palette_conversion_shaders[static_cast<int>(palette_format)]);

  VkRect2D region = {{0, 0}, {dst_entry->GetWidth(), dst_entry->GetHeight()}};
  draw.BeginRenderPass(destination_texture->GetFramebuffer(), region);

  PSUniformBlock uniforms = {};
  uniforms.multiplier = src_entry->format == TextureFormat::I4 ? 15.0f : 255.0f;
  uniforms.texel_buffer_offset = static_cast<int>(palette_offset / sizeof(u16));
  draw.SetPushConstants(&uniforms, sizeof(uniforms));
  draw.SetPSSampler(0, source_texture->GetRawTexIdentifier()->GetView(),
                    g_object_cache->GetPointSampler());
  draw.SetPSTexelBuffer(m_texel_buffer_view_r16_uint);
  draw.SetViewportAndScissor(0, 0, dst_entry->GetWidth(), dst_entry->GetHeight());
  draw.DrawWithoutVertexBuffer(4);
  draw.EndRenderPass();
}

void TextureConverter::EncodeTextureToMemory(VkImageView src_texture, u8* dest_ptr,
                                             const EFBCopyParams& params, u32 native_width,
                                             u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                                             const EFBRectangle& src_rect, bool scale_by_half)
{
  VkShaderModule shader = GetEncodingShader(params);
  if (shader == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Missing encoding fragment shader for format %u->%u",
              static_cast<unsigned>(params.efb_format), static_cast<unsigned>(params.copy_format));
    return;
  }

  // Can't do our own draw within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  m_encoding_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_PUSH_CONSTANT),
                         m_encoding_render_pass, g_shader_cache->GetScreenQuadVertexShader(),
                         VK_NULL_HANDLE, shader);

  // Uniform - int4 of left,top,native_width,scale
  s32 position_uniform[4] = {src_rect.left, src_rect.top, static_cast<s32>(native_width),
                             scale_by_half ? 2 : 1};
  draw.SetPushConstants(position_uniform, sizeof(position_uniform));

  // We also linear filtering for both box filtering and downsampling higher resolutions to 1x
  // TODO: This only produces perfect downsampling for 1.5x and 2x IR, other resolution will
  //       need more complex down filtering to average all pixels and produce the correct result.
  bool linear_filter = (scale_by_half && !params.depth) || g_renderer->GetEFBScale() != 1;
  draw.SetPSSampler(0, src_texture, linear_filter ? g_object_cache->GetLinearSampler() :
                                                    g_object_cache->GetPointSampler());

  u32 render_width = bytes_per_row / sizeof(u32);
  u32 render_height = num_blocks_y;
  Util::SetViewportAndScissor(g_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0, render_width,
                              render_height);

  VkRect2D render_region = {{0, 0}, {render_width, render_height}};
  draw.BeginRenderPass(m_encoding_render_framebuffer, render_region);
  draw.DrawWithoutVertexBuffer(4);
  draw.EndRenderPass();

  // Transition the image before copying
  m_encoding_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_encoding_download_texture->CopyFromImage(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), m_encoding_render_texture->GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, render_width, render_height, 0, 0);

  // Block until the GPU has finished copying to the staging texture.
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Copy from staging texture to the final destination, adjusting pitch if necessary.
  m_encoding_download_texture->ReadTexels(0, 0, render_width, render_height, dest_ptr,
                                          memory_stride);
}

void TextureConverter::EncodeTextureToMemoryYUYV(void* dst_ptr, u32 dst_width, u32 dst_stride,
                                                 u32 dst_height, Texture2D* src_texture,
                                                 const MathUtil::Rectangle<int>& src_rect)
{
  StateTracker::GetInstance()->EndRenderPass();

  // Borrow framebuffer from EFB2RAM encoder.
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  src_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_encoding_render_texture->TransitionToLayout(command_buffer,
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Use fragment shader to convert RGBA to YUYV.
  // Use linear sampler for downscaling. This texture is in BGRA order, so the data is already in
  // the order the guest is expecting and we don't have to swap it at readback time. The width
  // is halved because we're using an RGBA8 texture, but the YUYV data is two bytes per pixel.
  u32 output_width = dst_width / 2;
  UtilityShaderDraw draw(command_buffer,
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                         m_encoding_render_pass, g_shader_cache->GetPassthroughVertexShader(),
                         VK_NULL_HANDLE, m_rgb_to_yuyv_shader);
  VkRect2D region = {{0, 0}, {output_width, dst_height}};
  draw.BeginRenderPass(m_encoding_render_framebuffer, region);
  draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetLinearSampler());
  draw.DrawQuad(0, 0, static_cast<int>(output_width), static_cast<int>(dst_height), src_rect.left,
                src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                static_cast<int>(src_texture->GetWidth()),
                static_cast<int>(src_texture->GetHeight()));
  draw.EndRenderPass();

  // Render pass transitions to TRANSFER_SRC.
  m_encoding_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  // Copy from encoding texture to download buffer.
  m_encoding_download_texture->CopyFromImage(command_buffer, m_encoding_render_texture->GetImage(),
                                             VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, output_width,
                                             dst_height, 0, 0);
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Finally, copy to guest memory. This may have a different stride.
  m_encoding_download_texture->ReadTexels(0, 0, output_width, dst_height, dst_ptr, dst_stride);
}

void TextureConverter::DecodeYUYVTextureFromMemory(VKTexture* dst_texture, const void* src_ptr,
                                                   u32 src_width, u32 src_stride, u32 src_height)
{
  // Copies (and our decoding step) cannot be done inside a render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();

  // Pack each row without any padding in the texel buffer.
  size_t upload_stride = src_width * sizeof(u16);
  size_t upload_size = upload_stride * src_height;

  // Reserve space in the texel buffer for storing the raw image.
  if (!ReserveTexelBufferStorage(upload_size, sizeof(u16)))
    return;

  // Handle pitch differences here.
  if (src_stride != upload_stride)
  {
    const u8* src_row_ptr = reinterpret_cast<const u8*>(src_ptr);
    u8* dst_row_ptr = m_texel_buffer->GetCurrentHostPointer();
    size_t copy_size = std::min(upload_stride, static_cast<size_t>(src_stride));
    for (u32 row = 0; row < src_height; row++)
    {
      std::memcpy(dst_row_ptr, src_row_ptr, copy_size);
      src_row_ptr += src_stride;
      dst_row_ptr += upload_stride;
    }
  }
  else
  {
    std::memcpy(m_texel_buffer->GetCurrentHostPointer(), src_ptr, upload_size);
  }

  VkDeviceSize texel_buffer_offset = m_texel_buffer->GetCurrentOffset();
  m_texel_buffer->CommitMemory(upload_size);

  dst_texture->GetRawTexIdentifier()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // We divide the offset by 4 here because we're fetching RGBA8 elements.
  // The stride is in RGBA8 elements, so we divide by two because our data is two bytes per pixel.
  struct PSUniformBlock
  {
    int buffer_offset;
    int src_stride;
  };
  PSUniformBlock push_constants = {static_cast<int>(texel_buffer_offset / sizeof(u32)),
                                   static_cast<int>(src_width / 2)};

  // Convert from the YUYV data now in the intermediate texture to RGBA in the destination.
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_TEXTURE_CONVERSION),
                         m_encoding_render_pass, g_shader_cache->GetScreenQuadVertexShader(),
                         VK_NULL_HANDLE, m_yuyv_to_rgb_shader);
  VkRect2D region = {{0, 0}, {src_width, src_height}};
  draw.BeginRenderPass(dst_texture->GetFramebuffer(), region);
  draw.SetViewportAndScissor(0, 0, static_cast<int>(src_width), static_cast<int>(src_height));
  draw.SetPSTexelBuffer(m_texel_buffer_view_rgba8_unorm);
  draw.SetPushConstants(&push_constants, sizeof(push_constants));
  draw.DrawWithoutVertexBuffer(4);
  draw.EndRenderPass();
}

bool TextureConverter::SupportsTextureDecoding(TextureFormat format, TLUTFormat palette_format)
{
  auto key = std::make_pair(format, palette_format);
  auto iter = m_decoding_pipelines.find(key);
  if (iter != m_decoding_pipelines.end())
    return iter->second.valid;

  TextureDecodingPipeline pipeline;
  pipeline.base_info = TextureConversionShader::GetDecodingShaderInfo(format);
  pipeline.compute_shader = VK_NULL_HANDLE;
  pipeline.valid = false;

  if (!pipeline.base_info)
  {
    m_decoding_pipelines.emplace(key, pipeline);
    return false;
  }

  std::string shader_source =
      TextureConversionShader::GenerateDecodingShader(format, palette_format, APIType::Vulkan);

  pipeline.compute_shader = Util::CompileAndCreateComputeShader(shader_source);
  if (pipeline.compute_shader == VK_NULL_HANDLE)
  {
    m_decoding_pipelines.emplace(key, pipeline);
    return false;
  }

  pipeline.valid = true;
  m_decoding_pipelines.emplace(key, pipeline);
  return true;
}

void TextureConverter::DecodeTexture(VkCommandBuffer command_buffer,
                                     TextureCache::TCacheEntry* entry, u32 dst_level,
                                     const u8* data, size_t data_size, TextureFormat format,
                                     u32 width, u32 height, u32 aligned_width, u32 aligned_height,
                                     u32 row_stride, const u8* palette, TLUTFormat palette_format)
{
  VKTexture* destination_texture = static_cast<VKTexture*>(entry->texture.get());
  auto key = std::make_pair(format, palette_format);
  auto iter = m_decoding_pipelines.find(key);
  if (iter == m_decoding_pipelines.end())
    return;

  struct PushConstants
  {
    u32 dst_size[2];
    u32 src_size[2];
    u32 src_offset;
    u32 src_row_stride;
    u32 palette_offset;
  };

  // Copy to GPU-visible buffer, aligned to the data type
  auto info = iter->second;
  u32 bytes_per_buffer_elem =
      TextureConversionShader::GetBytesPerBufferElement(info.base_info->buffer_format);

  // Calculate total data size, including palette.
  // Only copy palette if it is required.
  u32 total_upload_size = static_cast<u32>(data_size);
  u32 palette_size = iter->second.base_info->palette_size;
  u32 palette_offset = total_upload_size;
  bool has_palette = palette_size > 0;
  if (has_palette)
  {
    // Align to u16.
    if ((total_upload_size % sizeof(u16)) != 0)
    {
      total_upload_size++;
      palette_offset++;
    }

    total_upload_size += palette_size;
  }

  // Allocate space for upload, if it fails, execute the buffer.
  if (!m_texel_buffer->ReserveMemory(total_upload_size, bytes_per_buffer_elem))
  {
    Util::ExecuteCurrentCommandsAndRestoreState(true, false);
    if (!m_texel_buffer->ReserveMemory(total_upload_size, bytes_per_buffer_elem))
      PanicAlert("Failed to reserve memory for encoded texture upload");
  }

  // Copy/commit upload buffer.
  u32 texel_buffer_offset = static_cast<u32>(m_texel_buffer->GetCurrentOffset());
  std::memcpy(m_texel_buffer->GetCurrentHostPointer(), data, data_size);
  if (has_palette)
    std::memcpy(m_texel_buffer->GetCurrentHostPointer() + palette_offset, palette, palette_size);
  m_texel_buffer->CommitMemory(total_upload_size);

  // Determine uniforms.
  PushConstants constants = {
      {width, height},
      {aligned_width, aligned_height},
      texel_buffer_offset / bytes_per_buffer_elem,
      row_stride / bytes_per_buffer_elem,
      static_cast<u32>((texel_buffer_offset + palette_offset) / sizeof(u16))};

  // Determine view to use for texel buffers.
  VkBufferView data_view = VK_NULL_HANDLE;
  switch (iter->second.base_info->buffer_format)
  {
  case TextureConversionShader::BUFFER_FORMAT_R8_UINT:
    data_view = m_texel_buffer_view_r8_uint;
    break;
  case TextureConversionShader::BUFFER_FORMAT_R16_UINT:
    data_view = m_texel_buffer_view_r16_uint;
    break;
  case TextureConversionShader::BUFFER_FORMAT_R32G32_UINT:
    data_view = m_texel_buffer_view_r32g32_uint;
    break;
  default:
    break;
  }

  // Dispatch compute to temporary texture.
  ComputeShaderDispatcher dispatcher(command_buffer,
                                     g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_COMPUTE),
                                     iter->second.compute_shader);
  m_decoding_texture->TransitionToLayout(command_buffer, Texture2D::ComputeImageLayout::WriteOnly);
  dispatcher.SetPushConstants(&constants, sizeof(constants));
  dispatcher.SetStorageImage(m_decoding_texture->GetView(), m_decoding_texture->GetLayout());
  dispatcher.SetTexelBuffer(0, data_view);
  if (has_palette)
    dispatcher.SetTexelBuffer(1, m_texel_buffer_view_r16_uint);
  auto groups = TextureConversionShader::GetDispatchCount(iter->second.base_info, aligned_width,
                                                          aligned_height);
  dispatcher.Dispatch(groups.first, groups.second, 1);

  // Copy from temporary texture to final destination.
  Texture2D* vulkan_tex_identifier = destination_texture->GetRawTexIdentifier();
  m_decoding_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vulkan_tex_identifier->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VkImageCopy image_copy = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                            {0, 0, 0},
                            {VK_IMAGE_ASPECT_COLOR_BIT, dst_level, 0, 1},
                            {0, 0, 0},
                            {width, height, 1}};
  vkCmdCopyImage(command_buffer, m_decoding_texture->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vulkan_tex_identifier->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
}

bool TextureConverter::CreateTexelBuffer()
{
  // Prefer an 8MB buffer if possible, but use less if the device doesn't support this.
  // This buffer is potentially going to be addressed as R8s in the future, so we assume
  // that one element is one byte.
  m_texel_buffer_size =
      std::min(static_cast<size_t>(TEXTURE_CONVERSION_TEXEL_BUFFER_SIZE),
               static_cast<size_t>(g_vulkan_context->GetDeviceLimits().maxTexelBufferElements));

  m_texel_buffer = StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
                                        m_texel_buffer_size, m_texel_buffer_size);
  if (!m_texel_buffer)
    return false;

  // Create views of the formats that we will be using.
  m_texel_buffer_view_r8_uint = CreateTexelBufferView(VK_FORMAT_R8_UINT);
  m_texel_buffer_view_r16_uint = CreateTexelBufferView(VK_FORMAT_R16_UINT);
  m_texel_buffer_view_r32g32_uint = CreateTexelBufferView(VK_FORMAT_R32G32_UINT);
  m_texel_buffer_view_rgba8_unorm = CreateTexelBufferView(VK_FORMAT_R8G8B8A8_UNORM);
  return m_texel_buffer_view_r8_uint != VK_NULL_HANDLE &&
         m_texel_buffer_view_r16_uint != VK_NULL_HANDLE &&
         m_texel_buffer_view_r32g32_uint != VK_NULL_HANDLE &&
         m_texel_buffer_view_rgba8_unorm != VK_NULL_HANDLE;
}

VkBufferView TextureConverter::CreateTexelBufferView(VkFormat format) const
{
  // Create a view of the whole buffer, we'll offset our texel load into it
  VkBufferViewCreateInfo view_info = {
      VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                    // const void*                pNext
      0,                                          // VkBufferViewCreateFlags    flags
      m_texel_buffer->GetBuffer(),                // VkBuffer                   buffer
      format,                                     // VkFormat                   format
      0,                                          // VkDeviceSize               offset
      m_texel_buffer_size                         // VkDeviceSize               range
  };

  VkBufferView view;
  VkResult res = vkCreateBufferView(g_vulkan_context->GetDevice(), &view_info, nullptr, &view);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBufferView failed: ");
    return VK_NULL_HANDLE;
  }

  return view;
}

bool TextureConverter::CompilePaletteConversionShaders()
{
  static const char PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE[] = R"(
    layout(std140, push_constant) uniform PCBlock
    {
      float multiplier;
      int texture_buffer_offset;
    } PC;

    SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    TEXEL_BUFFER_BINDING(0) uniform usamplerBuffer samp1;

    layout(location = 0) in vec3 f_uv0;
    layout(location = 0) out vec4 ocol0;

    int Convert3To8(int v)
    {
      // Swizzle bits: 00000123 -> 12312312
      return (v << 5) | (v << 2) | (v >> 1);
    }
    int Convert4To8(int v)
    {
      // Swizzle bits: 00001234 -> 12341234
      return (v << 4) | v;
    }
    int Convert5To8(int v)
    {
      // Swizzle bits: 00012345 -> 12345123
      return (v << 3) | (v >> 2);
    }
    int Convert6To8(int v)
    {
      // Swizzle bits: 00123456 -> 12345612
      return (v << 2) | (v >> 4);
    }
    float4 DecodePixel_RGB5A3(int val)
    {
      int r,g,b,a;
      if ((val&0x8000) > 0)
      {
        r=Convert5To8((val>>10) & 0x1f);
        g=Convert5To8((val>>5 ) & 0x1f);
        b=Convert5To8((val    ) & 0x1f);
        a=0xFF;
      }
      else
      {
        a=Convert3To8((val>>12) & 0x7);
        r=Convert4To8((val>>8 ) & 0xf);
        g=Convert4To8((val>>4 ) & 0xf);
        b=Convert4To8((val    ) & 0xf);
      }
      return float4(r, g, b, a) / 255.0;
    }
    float4 DecodePixel_RGB565(int val)
    {
      int r, g, b, a;
      r = Convert5To8((val >> 11) & 0x1f);
      g = Convert6To8((val >> 5) & 0x3f);
      b = Convert5To8((val) & 0x1f);
      a = 0xFF;
      return float4(r, g, b, a) / 255.0;
    }
    float4 DecodePixel_IA8(int val)
    {
      int i = val & 0xFF;
      int a = val >> 8;
      return float4(i, i, i, a) / 255.0;
    }
    void main()
    {
      int src = int(round(texture(samp0, f_uv0).r * PC.multiplier));
      src = int(texelFetch(samp1, src + PC.texture_buffer_offset).r);
      src = ((src << 8) & 0xFF00) | (src >> 8);
      ocol0 = DECODE(src);
    }

  )";

  std::string palette_ia8_program = StringFromFormat("%s\n%s", "#define DECODE DecodePixel_IA8",
                                                     PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE);
  std::string palette_rgb565_program = StringFromFormat(
      "%s\n%s", "#define DECODE DecodePixel_RGB565", PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE);
  std::string palette_rgb5a3_program = StringFromFormat(
      "%s\n%s", "#define DECODE DecodePixel_RGB5A3", PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE);

  m_palette_conversion_shaders[static_cast<int>(TLUTFormat::IA8)] =
      Util::CompileAndCreateFragmentShader(palette_ia8_program);
  m_palette_conversion_shaders[static_cast<int>(TLUTFormat::RGB565)] =
      Util::CompileAndCreateFragmentShader(palette_rgb565_program);
  m_palette_conversion_shaders[static_cast<int>(TLUTFormat::RGB5A3)] =
      Util::CompileAndCreateFragmentShader(palette_rgb5a3_program);

  return m_palette_conversion_shaders[static_cast<int>(TLUTFormat::IA8)] != VK_NULL_HANDLE &&
         m_palette_conversion_shaders[static_cast<int>(TLUTFormat::RGB565)] != VK_NULL_HANDLE &&
         m_palette_conversion_shaders[static_cast<int>(TLUTFormat::RGB5A3)] != VK_NULL_HANDLE;
}

VkShaderModule TextureConverter::CompileEncodingShader(const EFBCopyParams& params)
{
  const char* shader = TextureConversionShader::GenerateEncodingShader(params, APIType::Vulkan);
  VkShaderModule module = Util::CompileAndCreateFragmentShader(shader);
  if (module == VK_NULL_HANDLE)
    PanicAlert("Failed to compile texture encoding shader.");

  return module;
}

VkShaderModule TextureConverter::GetEncodingShader(const EFBCopyParams& params)
{
  auto iter = m_encoding_shaders.find(params);
  if (iter != m_encoding_shaders.end())
    return iter->second;

  VkShaderModule shader = CompileEncodingShader(params);
  m_encoding_shaders.emplace(params, shader);
  return shader;
}

bool TextureConverter::CreateEncodingRenderPass()
{
  VkAttachmentDescription attachments[] = {
      {0, ENCODING_TEXTURE_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpass_descriptions[] = {{0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1,
                                                  color_attachment_references, nullptr, nullptr, 0,
                                                  nullptr}};

  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      static_cast<u32>(ArraySize(attachments)),
                                      attachments,
                                      static_cast<u32>(ArraySize(subpass_descriptions)),
                                      subpass_descriptions,
                                      0,
                                      nullptr};

  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &pass_info, nullptr,
                                    &m_encoding_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (Encode) failed: ");
    return false;
  }

  return true;
}

bool TextureConverter::CreateEncodingTexture()
{
  m_encoding_render_texture = Texture2D::Create(
      ENCODING_TEXTURE_WIDTH, ENCODING_TEXTURE_HEIGHT, 1, 1, ENCODING_TEXTURE_FORMAT,
      VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  if (!m_encoding_render_texture)
    return false;

  VkImageView framebuffer_attachments[] = {m_encoding_render_texture->GetView()};
  VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                              nullptr,
                                              0,
                                              m_encoding_render_pass,
                                              static_cast<u32>(ArraySize(framebuffer_attachments)),
                                              framebuffer_attachments,
                                              m_encoding_render_texture->GetWidth(),
                                              m_encoding_render_texture->GetHeight(),
                                              m_encoding_render_texture->GetLayers()};

  VkResult res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                                     &m_encoding_render_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  return true;
}

bool TextureConverter::CreateEncodingDownloadTexture()
{
  m_encoding_download_texture =
      StagingTexture2D::Create(STAGING_BUFFER_TYPE_READBACK, ENCODING_TEXTURE_WIDTH,
                               ENCODING_TEXTURE_HEIGHT, ENCODING_TEXTURE_FORMAT);

  return m_encoding_download_texture && m_encoding_download_texture->Map();
}

bool TextureConverter::CreateDecodingTexture()
{
  m_decoding_texture = Texture2D::Create(
      DECODING_TEXTURE_WIDTH, DECODING_TEXTURE_HEIGHT, 1, 1, VK_FORMAT_R8G8B8A8_UNORM,
      VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  if (!m_decoding_texture)
    return false;

  VkClearColorValue clear_value = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkImageSubresourceRange clear_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  m_decoding_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                       m_decoding_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &clear_value, 1, &clear_range);
  return true;
}

bool TextureConverter::CompileYUYVConversionShaders()
{
  static const char RGB_TO_YUYV_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray source;
    layout(location = 0) in vec3 uv0;
    layout(location = 1) in vec4 col0;
    layout(location = 0) out vec4 ocol0;

    const vec3 y_const = vec3(0.257,0.504,0.098);
    const vec3 u_const = vec3(-0.148,-0.291,0.439);
    const vec3 v_const = vec3(0.439,-0.368,-0.071);
    const vec4 const3 = vec4(0.0625,0.5,0.0625,0.5);

    void main()
    {
      vec3 c0 = texture(source, vec3(uv0.xy - dFdx(uv0.xy) * 0.25, 0.0)).rgb;
      vec3 c1 = texture(source, vec3(uv0.xy + dFdx(uv0.xy) * 0.25, 0.0)).rgb;
      vec3 c01 = (c0 + c1) * 0.5;
      ocol0 = vec4(dot(c1, y_const),
                   dot(c01,u_const),
                   dot(c0,y_const),
                   dot(c01, v_const)) + const3;
    }
  )";

  static const char YUYV_TO_RGB_SHADER_SOURCE[] = R"(
    layout(std140, push_constant) uniform PCBlock
    {
      int buffer_offset;
      int src_stride;
    } PC;

    TEXEL_BUFFER_BINDING(0) uniform samplerBuffer source;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      ivec2 uv = ivec2(gl_FragCoord.xy);
      int buffer_pos = PC.buffer_offset + uv.y * PC.src_stride + (uv.x / 2);
      vec4 c0 = texelFetch(source, buffer_pos);

      float y = mix(c0.r, c0.b, (uv.x & 1) == 1);
      float yComp = 1.164 * (y - 0.0625);
      float uComp = c0.g - 0.5;
      float vComp = c0.a - 0.5;
      ocol0 = vec4(yComp + (1.596 * vComp),
                   yComp - (0.813 * vComp) - (0.391 * uComp),
                   yComp + (2.018 * uComp),
                   1.0);
    }
  )";

  std::string header = g_shader_cache->GetUtilityShaderHeader();
  std::string source = header + RGB_TO_YUYV_SHADER_SOURCE;
  m_rgb_to_yuyv_shader = Util::CompileAndCreateFragmentShader(source);
  source = header + YUYV_TO_RGB_SHADER_SOURCE;
  m_yuyv_to_rgb_shader = Util::CompileAndCreateFragmentShader(source);

  return m_rgb_to_yuyv_shader != VK_NULL_HANDLE && m_yuyv_to_rgb_shader != VK_NULL_HANDLE;
}

}  // namespace Vulkan
