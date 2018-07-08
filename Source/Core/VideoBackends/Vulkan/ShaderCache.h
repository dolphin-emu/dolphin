// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/LinearDiskCache.h"

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCompiler.h"

#include "VideoCommon/RenderState.h"

namespace Vulkan
{
class CommandBufferManager;
class VertexFormat;
class StreamBuffer;

struct PipelineInfo
{
  // These are packed in descending order of size, to avoid any padding so that the structure
  // can be copied/compared as a single block of memory. 64-bit pointer size is assumed.
  const VertexFormat* vertex_format;
  VkPipelineLayout pipeline_layout;
  VkShaderModule vs;
  VkShaderModule gs;
  VkShaderModule ps;
  VkRenderPass render_pass;
  BlendingState blend_state;
  RasterizationState rasterization_state;
  DepthState depth_state;
  MultisamplingState multisampling_state;
};

struct PipelineInfoHash
{
  std::size_t operator()(const PipelineInfo& key) const;
};

bool operator==(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator!=(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator<(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator>(const PipelineInfo& lhs, const PipelineInfo& rhs);

struct ComputePipelineInfo
{
  VkPipelineLayout pipeline_layout;
  VkShaderModule cs;
};

struct ComputePipelineInfoHash
{
  std::size_t operator()(const ComputePipelineInfo& key) const;
};

bool operator==(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs);
bool operator!=(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs);
bool operator<(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs);
bool operator>(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs);

class ShaderCache
{
public:
  ShaderCache();
  ~ShaderCache();

  // Get utility shader header based on current config.
  std::string GetUtilityShaderHeader() const;

  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();
  void Shutdown();

  // Creates a pipeline for the specified description. The resulting pipeline, if successful
  // is not stored anywhere, this is left up to the caller.
  VkPipeline CreatePipeline(const PipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it.
  VkPipeline GetPipeline(const PipelineInfo& info);

  // Creates a compute pipeline, and does not track the handle.
  VkPipeline CreateComputePipeline(const ComputePipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it
  VkPipeline GetComputePipeline(const ComputePipelineInfo& info);

  // Clears our pipeline cache of all objects. This is necessary when recompiling shaders,
  // as drivers are free to return the same pointer again, which means that we may end up using
  // and old pipeline object if they are not cleared first. Some stutter may be experienced
  // while our cache is rebuilt on use, but the pipeline cache object should mitigate this.
  // NOTE: Ensure that none of these objects are in use before calling.
  void ClearPipelineCache();

  // Saves the pipeline cache to disk. Call when shutting down.
  void SavePipelineCache();

  // Recompile shared shaders, call when stereo mode changes.
  void RecompileSharedShaders();

  // Reload pipeline cache. This will destroy all pipelines.
  void ReloadPipelineCache();

  // Shared shader accessors
  VkShaderModule GetScreenQuadVertexShader() const { return m_screen_quad_vertex_shader; }
  VkShaderModule GetPassthroughVertexShader() const { return m_passthrough_vertex_shader; }
  VkShaderModule GetScreenQuadGeometryShader() const { return m_screen_quad_geometry_shader; }
  VkShaderModule GetPassthroughGeometryShader() const { return m_passthrough_geometry_shader; }

private:
  bool CreatePipelineCache();
  bool LoadPipelineCache();
  bool ValidatePipelineCache(const u8* data, size_t data_length);
  void DestroyPipelineCache();
  bool CompileSharedShaders();
  void DestroySharedShaders();

  std::unordered_map<PipelineInfo, VkPipeline, PipelineInfoHash> m_pipeline_objects;
  std::unordered_map<ComputePipelineInfo, VkPipeline, ComputePipelineInfoHash>
      m_compute_pipeline_objects;
  VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
  std::string m_pipeline_cache_filename;

  // Utility/shared shaders
  VkShaderModule m_screen_quad_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_passthrough_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_screen_quad_geometry_shader = VK_NULL_HANDLE;
  VkShaderModule m_passthrough_geometry_shader = VK_NULL_HANDLE;
};

extern std::unique_ptr<ShaderCache> g_shader_cache;

}  // namespace Vulkan
