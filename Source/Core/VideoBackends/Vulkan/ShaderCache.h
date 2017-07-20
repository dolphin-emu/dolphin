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

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Vulkan
{
class CommandBufferManager;
class VertexFormat;
class StreamBuffer;

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

class ShaderCache
{
public:
  ShaderCache();
  ~ShaderCache();

  // Get utility shader header based on current config.
  std::string GetUtilityShaderHeader() const;

  // Accesses ShaderGen shader caches
  VkShaderModule GetVertexShaderForUid(const VertexShaderUid& uid);
  VkShaderModule GetGeometryShaderForUid(const GeometryShaderUid& uid);
  VkShaderModule GetPixelShaderForUid(const PixelShaderUid& uid);

  // Ubershader caches
  VkShaderModule GetVertexUberShaderForUid(const UberShader::VertexShaderUid& uid);
  VkShaderModule GetPixelUberShaderForUid(const UberShader::PixelShaderUid& uid);

  // Accesses ShaderGen shader caches asynchronously
  std::pair<VkShaderModule, bool> GetVertexShaderForUidAsync(const VertexShaderUid& uid);
  std::pair<VkShaderModule, bool> GetPixelShaderForUidAsync(const PixelShaderUid& uid);

  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();
  void Shutdown();

  // Creates a pipeline for the specified description. The resulting pipeline, if successful
  // is not stored anywhere, this is left up to the caller.
  VkPipeline CreatePipeline(const PipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it.
  VkPipeline GetPipeline(const PipelineInfo& info);

  // Find a pipeline by the specified description, if not found, attempts to create it. If this
  // resulted in a pipeline being created, the second field of the return value will be false,
  // otherwise for a cache hit it will be true.
  std::pair<VkPipeline, bool> GetPipelineWithCacheResult(const PipelineInfo& info);
  std::pair<std::pair<VkPipeline, bool>, bool>
  GetPipelineWithCacheResultAsync(const PipelineInfo& info);

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
  void ReloadShaderAndPipelineCaches();

  // Shared shader accessors
  VkShaderModule GetScreenQuadVertexShader() const { return m_screen_quad_vertex_shader; }
  VkShaderModule GetPassthroughVertexShader() const { return m_passthrough_vertex_shader; }
  VkShaderModule GetScreenQuadGeometryShader() const { return m_screen_quad_geometry_shader; }
  VkShaderModule GetPassthroughGeometryShader() const { return m_passthrough_geometry_shader; }
  void PrecompileUberShaders();
  void WaitForBackgroundCompilesToComplete();
  void RetrieveAsyncShaders();

private:
  bool CreatePipelineCache();
  bool LoadPipelineCache();
  bool ValidatePipelineCache(const u8* data, size_t data_length);
  void DestroyPipelineCache();
  void LoadShaderCaches();
  void DestroyShaderCaches();
  bool CompileSharedShaders();
  void DestroySharedShaders();

  // We generate a dummy pipeline with some defaults in the blend/depth states,
  // that way the driver is forced to compile something (looking at you, NVIDIA).
  // It can then hopefully re-use part of this pipeline for others in the future.
  void CreateDummyPipeline(const UberShader::VertexShaderUid& vuid, const GeometryShaderUid& guid,
                           const UberShader::PixelShaderUid& puid);

  template <typename Uid>
  struct ShaderModuleCache
  {
    std::map<Uid, std::pair<VkShaderModule, bool>> shader_map;
    LinearDiskCache<Uid, u32> disk_cache;
  };
  ShaderModuleCache<VertexShaderUid> m_vs_cache;
  ShaderModuleCache<GeometryShaderUid> m_gs_cache;
  ShaderModuleCache<PixelShaderUid> m_ps_cache;
  ShaderModuleCache<UberShader::VertexShaderUid> m_uber_vs_cache;
  ShaderModuleCache<UberShader::PixelShaderUid> m_uber_ps_cache;

  std::unordered_map<PipelineInfo, std::pair<VkPipeline, bool>, PipelineInfoHash>
      m_pipeline_objects;
  std::unordered_map<ComputePipelineInfo, VkPipeline, ComputePipelineInfoHash>
      m_compute_pipeline_objects;
  VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
  std::string m_pipeline_cache_filename;

  // Utility/shared shaders
  VkShaderModule m_screen_quad_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_passthrough_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_screen_quad_geometry_shader = VK_NULL_HANDLE;
  VkShaderModule m_passthrough_geometry_shader = VK_NULL_HANDLE;

  std::unique_ptr<VideoCommon::AsyncShaderCompiler> m_async_shader_compiler;

  // TODO: Use templates to reduce the number of these classes.
  class VertexShaderCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    VertexShaderCompilerWorkItem(const VertexShaderUid& uid) : m_uid(uid) {}
    bool Compile() override;
    void Retrieve() override;

  private:
    VertexShaderUid m_uid;
    ShaderCompiler::SPIRVCodeVector m_spirv;
    VkShaderModule m_module = VK_NULL_HANDLE;
  };
  class PixelShaderCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PixelShaderCompilerWorkItem(const PixelShaderUid& uid) : m_uid(uid) {}
    bool Compile() override;
    void Retrieve() override;

  private:
    PixelShaderUid m_uid;
    ShaderCompiler::SPIRVCodeVector m_spirv;
    VkShaderModule m_module = VK_NULL_HANDLE;
  };
  class PipelineCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PipelineCompilerWorkItem(const PipelineInfo& info) : m_info(info) {}
    bool Compile() override;
    void Retrieve() override;

  private:
    PipelineInfo m_info;
    VkPipeline m_pipeline;
  };
};

extern std::unique_ptr<ShaderCache> g_shader_cache;

}  // namespace Vulkan
