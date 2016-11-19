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
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"

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

  if (m_texel_buffer_view_r16_uint != VK_NULL_HANDLE)
    vkDestroyBufferView(g_vulkan_context->GetDevice(), m_texel_buffer_view_r16_uint, nullptr);

  if (m_encoding_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_encoding_render_pass, nullptr);

  if (m_encoding_render_framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_encoding_render_framebuffer, nullptr);

  for (VkShaderModule shader : m_encoding_shaders)
  {
    if (shader != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), shader, nullptr);
  }
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

  if (!CompileEncodingShaders())
  {
    PanicAlert("Failed to compile texture encoding shaders");
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

  return true;
}

void TextureConverter::ConvertTexture(TextureCache::TCacheEntry* dst_entry,
                                      TextureCache::TCacheEntry* src_entry,
                                      VkRenderPass render_pass, const void* palette,
                                      TlutFormat palette_format)
{
  struct PSUniformBlock
  {
    float multiplier;
    int texel_buffer_offset;
    int pad[2];
  };

  _assert_(static_cast<size_t>(palette_format) < NUM_PALETTE_CONVERSION_SHADERS);
  _assert_(dst_entry->config.rendertarget);

  // We want to align to 2 bytes (R16) or the device's texel buffer alignment, whichever is greater.
  VkDeviceSize texel_buffer_alignment =
      std::min(g_vulkan_context->GetTexelBufferAlignment(), sizeof(u16));
  size_t palette_size = (src_entry->format & 0xF) == GX_TF_I4 ? 32 : 512;

  // Allocate memory for the palette, and descriptor sets for the buffer.
  // If any of these fail, execute a command buffer, and try again.
  if (!m_texel_buffer->ReserveMemory(palette_size, texel_buffer_alignment))
  {
    WARN_LOG(VIDEO, "Executing command list while waiting for space in palette buffer");
    Util::ExecuteCurrentCommandsAndRestoreState(false);

    if (!m_texel_buffer->ReserveMemory(palette_size, texel_buffer_alignment))
    {
      PanicAlert("Failed to allocate space for texture conversion");
      return;
    }
  }

  // Copy in palette to texel buffer.
  u32 palette_offset = static_cast<u32>(m_texel_buffer->GetCurrentOffset());
  memcpy(m_texel_buffer->GetCurrentHostPointer(), palette, palette_size);
  m_texel_buffer->CommitMemory(palette_size);

  // EFB copies can be used as paletted textures as well. For these, we can't assume them to be
  // contain the correct data before the frame begins (when the init command buffer is executed),
  // so we must convert them at the appropriate time, during the drawing command buffer.
  VkCommandBuffer command_buffer;
  if (src_entry->IsEfbCopy())
  {
    command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
    StateTracker::GetInstance()->EndRenderPass();
    StateTracker::GetInstance()->SetPendingRebind();
  }
  else
  {
    // Use initialization command buffer and perform conversion before the drawing commands.
    command_buffer = g_command_buffer_mgr->GetCurrentInitCommandBuffer();
  }

  // Bind and draw to the destination.
  UtilityShaderDraw draw(command_buffer,
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_TEXTURE_CONVERSION),
                         render_pass, g_object_cache->GetScreenQuadVertexShader(), VK_NULL_HANDLE,
                         m_palette_conversion_shaders[palette_format]);

  VkRect2D region = {{0, 0}, {dst_entry->config.width, dst_entry->config.height}};
  draw.BeginRenderPass(dst_entry->GetFramebuffer(), region);

  PSUniformBlock uniforms = {};
  uniforms.multiplier = (src_entry->format & 0xF) == GX_TF_I4 ? 15.0f : 255.0f;
  uniforms.texel_buffer_offset = static_cast<int>(palette_offset / sizeof(u16));
  draw.SetPushConstants(&uniforms, sizeof(uniforms));
  draw.SetPSSampler(0, src_entry->GetTexture()->GetView(), g_object_cache->GetPointSampler());
  draw.SetPSTexelBuffer(m_texel_buffer_view_r16_uint);
  draw.SetViewportAndScissor(0, 0, dst_entry->config.width, dst_entry->config.height);
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();
}

void TextureConverter::EncodeTextureToMemory(VkImageView src_texture, u8* dest_ptr, u32 format,
                                             u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
                                             u32 memory_stride, PEControl::PixelFormat src_format,
                                             bool is_intensity, int scale_by_half,
                                             const EFBRectangle& src_rect)
{
  if (m_encoding_shaders[format] == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Missing encoding fragment shader for format %u", format);
    return;
  }

  // Can't do our own draw within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_PUSH_CONSTANT),
                         m_encoding_render_pass, g_object_cache->GetScreenQuadVertexShader(),
                         VK_NULL_HANDLE, m_encoding_shaders[format]);

  // Uniform - int4 of left,top,native_width,scale
  s32 position_uniform[4] = {src_rect.left, src_rect.top, static_cast<s32>(native_width),
                             scale_by_half ? 2 : 1};
  draw.SetPushConstants(position_uniform, sizeof(position_uniform));

  // Doesn't make sense to linear filter depth values
  draw.SetPSSampler(0, src_texture, (scale_by_half && src_format != PEControl::Z24) ?
                                        g_object_cache->GetLinearSampler() :
                                        g_object_cache->GetPointSampler());

  u32 render_width = bytes_per_row / sizeof(u32);
  u32 render_height = num_blocks_y;
  Util::SetViewportAndScissor(g_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0, render_width,
                              render_height);

  VkRect2D render_region = {{0, 0}, {render_width, render_height}};
  draw.BeginRenderPass(m_encoding_render_framebuffer, render_region);
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();

  // Transition the image before copying
  m_encoding_render_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_encoding_download_texture->CopyFromImage(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), m_encoding_render_texture->GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, render_width, render_height, 0, 0);

  // Block until the GPU has finished copying to the staging texture.
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Copy from staging texture to the final destination, adjusting pitch if necessary.
  m_encoding_download_texture->ReadTexels(0, 0, render_width, render_height, dest_ptr,
                                          memory_stride);
}

bool TextureConverter::CreateTexelBuffer()
{
  // Prefer an 8MB buffer if possible, but use less if the device doesn't support this.
  // This buffer is potentially going to be addressed as R8s in the future, so we assume
  // that one element is one byte.
  m_texel_buffer_size =
      std::min(TEXTURE_CONVERSION_TEXEL_BUFFER_SIZE,
               static_cast<size_t>(g_vulkan_context->GetDeviceLimits().maxTexelBufferElements));

  m_texel_buffer = StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
                                        m_texel_buffer_size, m_texel_buffer_size);
  if (!m_texel_buffer)
    return false;

  // Create views of the formats that we will be using.
  m_texel_buffer_view_r16_uint = CreateTexelBufferView(VK_FORMAT_R16_UINT);
  return m_texel_buffer_view_r16_uint != VK_NULL_HANDLE;
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

  m_palette_conversion_shaders[GX_TL_IA8] =
      Util::CompileAndCreateFragmentShader(palette_ia8_program);
  m_palette_conversion_shaders[GX_TL_RGB565] =
      Util::CompileAndCreateFragmentShader(palette_rgb565_program);
  m_palette_conversion_shaders[GX_TL_RGB5A3] =
      Util::CompileAndCreateFragmentShader(palette_rgb5a3_program);

  return m_palette_conversion_shaders[GX_TL_IA8] != VK_NULL_HANDLE &&
         m_palette_conversion_shaders[GX_TL_RGB565] != VK_NULL_HANDLE &&
         m_palette_conversion_shaders[GX_TL_RGB5A3] != VK_NULL_HANDLE;
}

bool TextureConverter::CompileEncodingShaders()
{
  // Texture encoding shaders
  static const u32 texture_encoding_shader_formats[] = {
      GX_TF_I4,   GX_TF_I8,   GX_TF_IA4,  GX_TF_IA8,  GX_TF_RGB565, GX_TF_RGB5A3, GX_TF_RGBA8,
      GX_CTF_R4,  GX_CTF_RA4, GX_CTF_RA8, GX_CTF_A8,  GX_CTF_R8,    GX_CTF_G8,    GX_CTF_B8,
      GX_CTF_RG8, GX_CTF_GB8, GX_CTF_Z8H, GX_TF_Z8,   GX_CTF_Z16R,  GX_TF_Z16,    GX_TF_Z24X8,
      GX_CTF_Z4,  GX_CTF_Z8M, GX_CTF_Z8L, GX_CTF_Z16L};
  for (u32 format : texture_encoding_shader_formats)
  {
    const char* shader_source =
        TextureConversionShader::GenerateEncodingShader(format, APIType::Vulkan);
    m_encoding_shaders[format] = Util::CompileAndCreateFragmentShader(shader_source);
    if (m_encoding_shaders[format] == VK_NULL_HANDLE)
      return false;
  }

  return true;
}

bool TextureConverter::CreateEncodingRenderPass()
{
  VkAttachmentDescription attachments[] = {
      {0, ENCODING_TEXTURE_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL}};

  VkAttachmentReference color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpass_descriptions[] = {{0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1,
                                                  color_attachment_references, nullptr, nullptr, 0,
                                                  nullptr}};

  VkSubpassDependency dependancies[] = {
      {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
       VK_PIPELINE_STAGE_TRANSFER_BIT,
       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
       VK_ACCESS_TRANSFER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT}};

  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      static_cast<u32>(ArraySize(attachments)),
                                      attachments,
                                      static_cast<u32>(ArraySize(subpass_descriptions)),
                                      subpass_descriptions,
                                      static_cast<u32>(ArraySize(dependancies)),
                                      dependancies};

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

}  // namespace Vulkan
