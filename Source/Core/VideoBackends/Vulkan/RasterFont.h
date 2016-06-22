// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class CommandBufferManager;
class ObjectCache;
class StateTracker;
class Texture2D;

class RasterFont
{
public:
  RasterFont(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr);
  ~RasterFont();

  bool Initialize();

  void PrintMultiLineText(VkRenderPass render_pass, const std::string& text, float start_x,
                          float start_y, u32 bbWidth, u32 bbHeight, u32 color);

private:
  bool CreateTexture();
  bool CreateShaders();

  ObjectCache* m_object_cache;
  CommandBufferManager* m_command_buffer_mgr;

  std::unique_ptr<Texture2D> m_texture;

  VkShaderModule m_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_fragment_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
