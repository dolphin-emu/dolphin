// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/PaletteTextureConverter.h"

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
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
PaletteTextureConverter::PaletteTextureConverter()
{
}

PaletteTextureConverter::~PaletteTextureConverter()
{
  for (const auto& it : m_shaders)
  {
    if (it != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), it, nullptr);
  }

  if (m_palette_buffer_view != VK_NULL_HANDLE)
    vkDestroyBufferView(g_vulkan_context->GetDevice(), m_palette_buffer_view, nullptr);

  if (m_pipeline_layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(g_vulkan_context->GetDevice(), m_pipeline_layout, nullptr);

  if (m_palette_set_layout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(g_vulkan_context->GetDevice(), m_palette_set_layout, nullptr);
}

bool PaletteTextureConverter::Initialize()
{
  if (!CreateBuffers())
    return false;

  if (!CompileShaders())
    return false;

  if (!CreateDescriptorLayout())
    return false;

  return true;
}

void PaletteTextureConverter::ConvertTexture(StateTracker* state_tracker,
                                             VkCommandBuffer command_buffer,
                                             VkRenderPass render_pass,
                                             VkFramebuffer dst_framebuffer, Texture2D* src_texture,
                                             u32 width, u32 height, void* palette,
                                             TlutFormat format, u32 src_format)
{
  struct PSUniformBlock
  {
    float multiplier;
    int texel_buffer_offset;
    int pad[2];
  };

  _assert_(static_cast<size_t>(format) < NUM_PALETTE_CONVERSION_SHADERS);

  size_t palette_size = (src_format & 0xF) == GX_TF_I4 ? 32 : 512;
  VkDescriptorSet texel_buffer_descriptor_set;

  // Allocate memory for the palette, and descriptor sets for the buffer.
  // If any of these fail, execute a command buffer, and try again.
  if (!m_palette_stream_buffer->ReserveMemory(palette_size,
                                              g_vulkan_context->GetTexelBufferAlignment()) ||
      (texel_buffer_descriptor_set =
           g_command_buffer_mgr->AllocateDescriptorSet(m_palette_set_layout)) == VK_NULL_HANDLE)
  {
    WARN_LOG(VIDEO, "Executing command list while waiting for space in palette buffer");
    Util::ExecuteCurrentCommandsAndRestoreState(state_tracker, false);

    if (!m_palette_stream_buffer->ReserveMemory(palette_size,
                                                g_vulkan_context->GetTexelBufferAlignment()) ||
        (texel_buffer_descriptor_set =
             g_command_buffer_mgr->AllocateDescriptorSet(m_palette_set_layout)) == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to allocate space for texture conversion");
      return;
    }
  }

  // Fill descriptor set #2 (texel buffer)
  u32 palette_offset = static_cast<u32>(m_palette_stream_buffer->GetCurrentOffset());
  VkWriteDescriptorSet texel_set_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                          nullptr,
                                          texel_buffer_descriptor_set,
                                          0,
                                          0,
                                          1,
                                          VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                          nullptr,
                                          nullptr,
                                          &m_palette_buffer_view};
  vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), 1, &texel_set_write, 0, nullptr);

  Util::BufferMemoryBarrier(command_buffer, m_palette_stream_buffer->GetBuffer(),
                            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, palette_offset,
                            palette_size, VK_PIPELINE_STAGE_HOST_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  // Set up draw
  UtilityShaderDraw draw(command_buffer, m_pipeline_layout, render_pass,
                         g_object_cache->GetScreenQuadVertexShader(), VK_NULL_HANDLE,
                         m_shaders[format]);

  VkRect2D region = {{0, 0}, {width, height}};
  draw.BeginRenderPass(dst_framebuffer, region);

  // Copy in palette
  memcpy(m_palette_stream_buffer->GetCurrentHostPointer(), palette, palette_size);
  m_palette_stream_buffer->CommitMemory(palette_size);

  // PS Uniforms/Samplers
  PSUniformBlock uniforms = {};
  uniforms.multiplier = (src_format & 0xF) == GX_TF_I4 ? 15.0f : 255.0f;
  uniforms.texel_buffer_offset = static_cast<int>(palette_offset / sizeof(u16));
  draw.SetPushConstants(&uniforms, sizeof(uniforms));
  draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());

  // We have to bind the texel buffer descriptor set separately.
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                          &texel_buffer_descriptor_set, 0, nullptr);

  // Draw
  draw.SetViewportAndScissor(0, 0, width, height);
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();
}

bool PaletteTextureConverter::CreateBuffers()
{
  // TODO: Check against maximum size
  static const size_t BUFFER_SIZE = 1024 * 1024;

  m_palette_stream_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, BUFFER_SIZE, BUFFER_SIZE);
  if (!m_palette_stream_buffer)
    return false;

  // Create a view of the whole buffer, we'll offset our texel load into it
  VkBufferViewCreateInfo view_info = {
      VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                    // const void*                pNext
      0,                                          // VkBufferViewCreateFlags    flags
      m_palette_stream_buffer->GetBuffer(),       // VkBuffer                   buffer
      VK_FORMAT_R16_UINT,                         // VkFormat                   format
      0,                                          // VkDeviceSize               offset
      BUFFER_SIZE                                 // VkDeviceSize               range
  };

  VkResult res = vkCreateBufferView(g_vulkan_context->GetDevice(), &view_info, nullptr,
                                    &m_palette_buffer_view);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBufferView failed: ");
    return false;
  }

  return true;
}

bool PaletteTextureConverter::CompileShaders()
{
  static const char PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE[] = R"(
    layout(std140, push_constant) uniform PCBlock
    {
      float multiplier;
      int texture_buffer_offset;
    } PC;

    layout(set = 1, binding = 0) uniform sampler2DArray samp0;
    layout(set = 0, binding = 0) uniform usamplerBuffer samp1;

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

  m_shaders[GX_TL_IA8] = Util::CompileAndCreateFragmentShader(palette_ia8_program);
  m_shaders[GX_TL_RGB565] = Util::CompileAndCreateFragmentShader(palette_rgb565_program);
  m_shaders[GX_TL_RGB5A3] = Util::CompileAndCreateFragmentShader(palette_rgb5a3_program);

  return (m_shaders[GX_TL_IA8] != VK_NULL_HANDLE && m_shaders[GX_TL_RGB565] != VK_NULL_HANDLE &&
          m_shaders[GX_TL_RGB5A3] != VK_NULL_HANDLE);
}

bool PaletteTextureConverter::CreateDescriptorLayout()
{
  static const VkDescriptorSetLayoutBinding set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  static const VkDescriptorSetLayoutCreateInfo set_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
      static_cast<u32>(ArraySize(set_bindings)), set_bindings};

  VkResult res = vkCreateDescriptorSetLayout(g_vulkan_context->GetDevice(), &set_info, nullptr,
                                             &m_palette_set_layout);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout failed: ");
    return false;
  }

  VkDescriptorSetLayout sets[] = {m_palette_set_layout, g_object_cache->GetDescriptorSetLayout(
                                                            DESCRIPTOR_SET_PIXEL_SHADER_SAMPLERS)};

  VkPushConstantRange push_constant_range = {
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, PUSH_CONSTANT_BUFFER_SIZE};

  VkPipelineLayoutCreateInfo pipeline_layout_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                     nullptr,
                                                     0,
                                                     static_cast<u32>(ArraySize(sets)),
                                                     sets,
                                                     1,
                                                     &push_constant_range};

  res = vkCreatePipelineLayout(g_vulkan_context->GetDevice(), &pipeline_layout_info, nullptr,
                               &m_pipeline_layout);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreatePipelineLayout failed: ");
    return false;
  }

  return true;
}

}  // namespace Vulkan
