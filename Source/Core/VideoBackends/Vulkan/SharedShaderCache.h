// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class SharedShaderCache
{
public:
  SharedShaderCache(VkDevice device, VertexShaderCache* vs_cache, GeometryShaderCache* gs_cache,
                    PixelShaderCache* ps_cache);
  ~SharedShaderCache();

  // Recompile static shaders, call when MSAA mode changes, etc.
  bool CompileShaders();
  void DestroyShaders();

  // Vertex Shaders
  VkShaderModule GetPassthroughVertexShader() const { return m_vertex_shaders.passthrough; }
  VkShaderModule GetScreenQuadVertexShader() const { return m_vertex_shaders.screen_quad; }
  // Geometry Shaders
  VkShaderModule GetPassthroughGeometryShader() const { return m_geometry_shaders.passthrough; }
  VkShaderModule GetScreenQuadGeometryShader() const { return m_geometry_shaders.screen_quad; }
  // Fragment Shaders
  VkShaderModule GetClearFragmentShader() const { return m_fragment_shaders.clear; }
  VkShaderModule GetCopyFragmentShader() const { return m_fragment_shaders.copy; }
  VkShaderModule GetColorMatrixFragmentShader() const { return m_fragment_shaders.color_matrix; }
  VkShaderModule GetDepthMatrixFragmentShader() const { return m_fragment_shaders.depth_matrix; }
private:
  VkDevice m_device;
  VertexShaderCache* m_vs_cache;
  GeometryShaderCache* m_gs_cache;
  PixelShaderCache* m_ps_cache;

  struct
  {
    VkShaderModule screen_quad = VK_NULL_HANDLE;
    VkShaderModule passthrough = VK_NULL_HANDLE;
  } m_vertex_shaders;

  struct
  {
    VkShaderModule screen_quad = VK_NULL_HANDLE;
    VkShaderModule passthrough = VK_NULL_HANDLE;
  } m_geometry_shaders;

  struct
  {
    VkShaderModule clear = VK_NULL_HANDLE;
    VkShaderModule copy = VK_NULL_HANDLE;
    VkShaderModule color_matrix = VK_NULL_HANDLE;
    VkShaderModule depth_matrix = VK_NULL_HANDLE;
  } m_fragment_shaders;
};

}  // namespace Vulkan
