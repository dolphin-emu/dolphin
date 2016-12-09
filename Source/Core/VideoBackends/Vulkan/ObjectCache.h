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

#include "Common/CommonTypes.h"
#include "Common/LinearDiskCache.h"

#include "VideoBackends/Vulkan/Constants.h"

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

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
  BlendState blend_state;
  RasterizationState rasterization_state;
  DepthStencilState depth_stencil_state;
  VkPrimitiveTopology primitive_topology;
};

struct PipelineInfoHash
{
  std::size_t operator()(const PipelineInfo& key) const;
};

bool operator==(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator!=(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator<(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator>(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator==(const SamplerState& lhs, const SamplerState& rhs);
bool operator!=(const SamplerState& lhs, const SamplerState& rhs);
bool operator>(const SamplerState& lhs, const SamplerState& rhs);
bool operator<(const SamplerState& lhs, const SamplerState& rhs);

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

class ObjectCache
{
public:
  ObjectCache();
  ~ObjectCache();

  // Descriptor set layout accessor. Used for allocating descriptor sets.
  VkDescriptorSetLayout GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT layout) const
  {
    return m_descriptor_set_layouts[layout];
  }
  // Pipeline layout accessor. Used to fill in required field in PipelineInfo.
  VkPipelineLayout GetPipelineLayout(PIPELINE_LAYOUT layout) const
  {
    return m_pipeline_layouts[layout];
  }
  // Shared utility shader resources
  VertexFormat* GetUtilityShaderVertexFormat() const
  {
    return m_utility_shader_vertex_format.get();
  }
  StreamBuffer* GetUtilityShaderVertexBuffer() const
  {
    return m_utility_shader_vertex_buffer.get();
  }
  StreamBuffer* GetUtilityShaderUniformBuffer() const
  {
    return m_utility_shader_uniform_buffer.get();
  }

  // Get utility shader header based on current config.
  std::string GetUtilityShaderHeader() const;

  // Accesses ShaderGen shader caches
  VkShaderModule GetVertexShaderForUid(const VertexShaderUid& uid);
  VkShaderModule GetGeometryShaderForUid(const GeometryShaderUid& uid);
  VkShaderModule GetPixelShaderForUid(const PixelShaderUid& uid);

  // Static samplers
  VkSampler GetPointSampler() const { return m_point_sampler; }
  VkSampler GetLinearSampler() const { return m_linear_sampler; }
  VkSampler GetSampler(const SamplerState& info);

  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();

  // Creates a pipeline for the specified description. The resulting pipeline, if successful
  // is not stored anywhere, this is left up to the caller.
  VkPipeline CreatePipeline(const PipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it.
  VkPipeline GetPipeline(const PipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it. If this
  // resulted in a pipeline being created, the second field of the return value will be false,
  // otherwise for a cache hit it will be true.
  std::pair<VkPipeline, bool> GetPipelineWithCacheResult(const PipelineInfo& info);

  // Creates a compute pipeline, and does not track the handle.
  VkPipeline CreateComputePipeline(const ComputePipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it
  VkPipeline GetComputePipeline(const ComputePipelineInfo& info);

  // Saves the pipeline cache to disk. Call when shutting down.
  void SavePipelineCache();

  // Clear sampler cache, use when anisotropy mode changes
  // WARNING: Ensure none of the objects from here are in use when calling
  void ClearSamplerCache();

  // Recompile shared shaders, call when stereo mode changes.
  void RecompileSharedShaders();

  // Shared shader accessors
  VkShaderModule GetScreenQuadVertexShader() const { return m_screen_quad_vertex_shader; }
  VkShaderModule GetPassthroughVertexShader() const { return m_passthrough_vertex_shader; }
  VkShaderModule GetScreenQuadGeometryShader() const { return m_screen_quad_geometry_shader; }
  VkShaderModule GetPassthroughGeometryShader() const { return m_passthrough_geometry_shader; }
  // Gets the filename of the specified type of cache object (e.g. vertex shader, pipeline).
  std::string GetDiskCacheFileName(const char* type);

private:
  bool CreatePipelineCache(bool load_from_disk);
  bool ValidatePipelineCache(const u8* data, size_t data_length);
  void DestroyPipelineCache();
  void LoadShaderCaches();
  void DestroyShaderCaches();
  bool CreateDescriptorSetLayouts();
  void DestroyDescriptorSetLayouts();
  bool CreatePipelineLayouts();
  void DestroyPipelineLayouts();
  bool CreateUtilityShaderVertexFormat();
  bool CreateStaticSamplers();
  bool CompileSharedShaders();
  void DestroySharedShaders();
  void DestroySamplers();

  std::array<VkDescriptorSetLayout, NUM_DESCRIPTOR_SET_LAYOUTS> m_descriptor_set_layouts = {};
  std::array<VkPipelineLayout, NUM_PIPELINE_LAYOUTS> m_pipeline_layouts = {};

  std::unique_ptr<VertexFormat> m_utility_shader_vertex_format;
  std::unique_ptr<StreamBuffer> m_utility_shader_vertex_buffer;
  std::unique_ptr<StreamBuffer> m_utility_shader_uniform_buffer;

  template <typename Uid>
  struct ShaderCache
  {
    std::map<Uid, VkShaderModule> shader_map;
    LinearDiskCache<Uid, u32> disk_cache;
  };
  ShaderCache<VertexShaderUid> m_vs_cache;
  ShaderCache<GeometryShaderUid> m_gs_cache;
  ShaderCache<PixelShaderUid> m_ps_cache;

  std::unordered_map<PipelineInfo, VkPipeline, PipelineInfoHash> m_pipeline_objects;
  std::unordered_map<ComputePipelineInfo, VkPipeline, ComputePipelineInfoHash>
      m_compute_pipeline_objects;
  VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
  std::string m_pipeline_cache_filename;

  VkSampler m_point_sampler = VK_NULL_HANDLE;
  VkSampler m_linear_sampler = VK_NULL_HANDLE;

  std::map<SamplerState, VkSampler> m_sampler_cache;

  // Utility/shared shaders
  VkShaderModule m_screen_quad_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_passthrough_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_screen_quad_geometry_shader = VK_NULL_HANDLE;
  VkShaderModule m_passthrough_geometry_shader = VK_NULL_HANDLE;
};

extern std::unique_ptr<ObjectCache> g_object_cache;

}  // namespace Vulkan
