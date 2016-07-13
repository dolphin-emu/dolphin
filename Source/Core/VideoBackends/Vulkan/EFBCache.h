// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

#include "VideoCommon/VideoCommon.h"

namespace Vulkan
{
class FramebufferManager;
class StagingTexture2D;
class StateTracker;
class Texture2D;

// TODO: Only read back blocks at a time, rather than the whole image.
class EFBCache
{
public:
  EFBCache(FramebufferManager* framebuffer_mgr);
  ~EFBCache();

  bool Initialize();

  u32 PeekEFBColor(StateTracker* state_tracker, u32 x, u32 y);
  float PeekEFBDepth(StateTracker* state_tracker, u32 x, u32 y);
  void InvalidatePeekCache();

  void PokeEFBColor(u32 x, u32 y, u32 color);
  void PokeEFBDepth(u32 x, u32 y, float depth);
  void FlushEFBPokes();

private:
  bool CreateRenderPasses();
  bool CompileShaders();
  bool CreateTextures();
  bool PopulateColorReadbackTexture(StateTracker* state_tracker);
  bool PopulateDepthReadbackTexture(StateTracker* state_tracker);

  FramebufferManager* m_framebuffer_mgr;

  std::unique_ptr<Texture2D> m_color_copy_texture;
  std::unique_ptr<Texture2D> m_depth_copy_texture;
  VkFramebuffer m_color_copy_framebuffer = VK_NULL_HANDLE;
  VkFramebuffer m_depth_copy_framebuffer = VK_NULL_HANDLE;

  std::unique_ptr<StagingTexture2D> m_color_readback_texture;
  std::unique_ptr<StagingTexture2D> m_depth_readback_texture;
  bool m_color_readback_texture_valid = false;
  bool m_depth_readback_texture_valid = false;

  std::vector<UtilityShaderVertex> m_poke_vertices;

  VkRenderPass m_copy_color_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_copy_depth_render_pass = VK_NULL_HANDLE;
  VkShaderModule m_copy_color_shader = VK_NULL_HANDLE;
  VkShaderModule m_copy_depth_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
