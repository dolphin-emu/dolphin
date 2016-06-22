// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/StreamBuffer.h"

#include "VideoCommon/TextureDecoder.h"

namespace Vulkan
{
class CommandBufferManager;
class ObjectCache;
class StateTracker;
class Texture2D;

// Since this converter uses a uniform texel buffer, we can't use the general pipeline generators.

class PaletteTextureConverter
{
public:
  PaletteTextureConverter(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr,
                          StateTracker* state_tracker);
  ~PaletteTextureConverter();

  bool Initialize();

  void ConvertTexture(Texture2D* dst_texture, VkFramebuffer dst_framebuffer, Texture2D* src_texture,
                      u32 width, u32 height, void* palette, TlutFormat format);

private:
  static const u32 NUM_PALETTE_CONVERSION_SHADERS = 3;

  bool CreateBuffers();
  bool CompileShaders();
  bool CreateRenderPass();
  bool CreateDescriptorLayout();
  bool CreatePipelines();

  ObjectCache* m_object_cache = nullptr;
  CommandBufferManager* m_command_buffer_mgr = nullptr;
  StateTracker* m_state_tracker = nullptr;

  VkRenderPass m_render_pass = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

  std::array<VkShaderModule, NUM_PALETTE_CONVERSION_SHADERS> m_shaders = {};
  std::array<VkPipeline, NUM_PALETTE_CONVERSION_SHADERS> m_pipelines = {};

  std::unique_ptr<StreamBuffer> m_palette_stream_buffer;
  VkBufferView m_palette_buffer_view = VK_NULL_HANDLE;

  std::unique_ptr<StreamBuffer> m_uniform_buffer;
};

}  // namespace Vulkan
