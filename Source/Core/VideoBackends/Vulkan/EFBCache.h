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
class StreamBuffer;
class Texture2D;
class VertexFormat;

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

  void PokeEFBColor(StateTracker* state_tracker, u32 x, u32 y, u32 color);
  void PokeEFBDepth(StateTracker* state_tracker, u32 x, u32 y, float depth);
  void FlushEFBPokes(StateTracker* state_tracker);

private:
  struct EFBPokeVertex
  {
    float position[4];
    u32 color;
  };

  bool CreateRenderPasses();
  bool CompileShaders();
  bool CreateTextures();
  void CreatePokeVertexFormat();
  bool CreatePokeVertexBuffer();
  bool PopulateColorReadbackTexture(StateTracker* state_tracker);
  bool PopulateDepthReadbackTexture(StateTracker* state_tracker);

  void CreatePokeVertices(std::vector<EFBPokeVertex>* destination_list, u32 x, u32 y, float z,
                          u32 color);

  void DrawPokeVertices(StateTracker* state_tracker, const EFBPokeVertex* vertices,
                        size_t vertex_count, bool write_color, bool write_depth);

  FramebufferManager* m_framebuffer_mgr;

  std::unique_ptr<Texture2D> m_color_copy_texture;
  std::unique_ptr<Texture2D> m_depth_copy_texture;
  VkFramebuffer m_color_copy_framebuffer = VK_NULL_HANDLE;
  VkFramebuffer m_depth_copy_framebuffer = VK_NULL_HANDLE;

  std::unique_ptr<StagingTexture2D> m_color_readback_texture;
  std::unique_ptr<StagingTexture2D> m_depth_readback_texture;
  bool m_color_readback_texture_valid = false;
  bool m_depth_readback_texture_valid = false;

  std::unique_ptr<VertexFormat> m_poke_vertex_format;
  std::unique_ptr<StreamBuffer> m_poke_vertex_stream_buffer;
  std::vector<EFBPokeVertex> m_color_poke_vertices;
  std::vector<EFBPokeVertex> m_depth_poke_vertices;
  VkPrimitiveTopology m_poke_primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkRenderPass m_copy_color_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_copy_depth_render_pass = VK_NULL_HANDLE;
  VkShaderModule m_copy_color_shader = VK_NULL_HANDLE;
  VkShaderModule m_copy_depth_shader = VK_NULL_HANDLE;

  VkShaderModule m_poke_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_poke_geometry_shader = VK_NULL_HANDLE;
  VkShaderModule m_poke_pixel_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
