// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoCommon/TextureDecoder.h"

namespace Vulkan
{
class StateTracker;
class Texture2D;

// Since this converter uses a uniform texel buffer, we can't use the general pipeline generators.

class PaletteTextureConverter
{
public:
  PaletteTextureConverter();
  ~PaletteTextureConverter();

  bool Initialize();

  void ConvertTexture(StateTracker* state_tracker, VkCommandBuffer command_buffer,
                      VkRenderPass render_pass, VkFramebuffer dst_framebuffer,
                      Texture2D* src_texture, u32 width, u32 height, void* palette,
                      TlutFormat format, u32 src_format);

private:
  static const size_t NUM_PALETTE_CONVERSION_SHADERS = 3;

  bool CreateBuffers();
  bool CompileShaders();
  bool CreateDescriptorLayout();

  VkDescriptorSetLayout m_palette_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

  std::array<VkShaderModule, NUM_PALETTE_CONVERSION_SHADERS> m_shaders = {};

  std::unique_ptr<StreamBuffer> m_palette_stream_buffer;
  VkBufferView m_palette_buffer_view = VK_NULL_HANDLE;

  std::unique_ptr<StreamBuffer> m_uniform_buffer;
};

}  // namespace Vulkan
