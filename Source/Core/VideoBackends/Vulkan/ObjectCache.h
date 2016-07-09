// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <unordered_map>

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/SharedShaderCache.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class CommandBufferManager;
class VertexFormat;
class StreamBuffer;

struct PipelineInfo
{
  const VertexFormat* vertex_format;
  VkPipelineLayout pipeline_layout;
  VkShaderModule vs;
  VkShaderModule gs;
  VkShaderModule ps;
  VkRenderPass render_pass;
  RasterizationState rasterization_state;
  DepthStencilState depth_stencil_state;
  BlendState blend_state;
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

class ObjectCache
{
public:
  ObjectCache(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device);
  ~ObjectCache();

  VkInstance GetVulkanInstance() const { return m_instance; }
  VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
  VkDevice GetDevice() const { return m_device; }
  const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const
  {
    return m_device_memory_properties;
  }
  const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_device_features; }
  const VkPhysicalDeviceLimits& GetDeviceLimits() const { return m_device_limits; }

  // Support bits
  bool SupportsAnisotropicFiltering() const { return (m_device_features.samplerAnisotropy == VK_TRUE); }
  bool SupportsGeometryShaders() const { return (m_device_features.geometryShader == VK_TRUE); }
  bool SupportsDualSourceBlend() const { return (m_device_features.dualSrcBlend == VK_TRUE); }
  bool SupportsLogicOps() const { return (m_device_features.logicOp == VK_TRUE); }

  // Helpers for getting constants
  VkDeviceSize GetUniformBufferAlignment() const
  {
    return m_device_limits.minUniformBufferOffsetAlignment;
  }
  VkDeviceSize GetTexelBufferAlignment() const
  {
    return m_device_limits.minUniformBufferOffsetAlignment;
  }
  VkDeviceSize GetTextureUploadAlignment() const
  {
    return m_device_limits.optimalBufferCopyOffsetAlignment;
  }
  VkDeviceSize GetTextureUploadPitchAlignment() const
  {
    return m_device_limits.optimalBufferCopyRowPitchAlignment;
  }

  // We have four shared pipeline layouts:
  //   - Standard
  //       - Per-stage UBO (VS/GS/PS, VS constants accessible from PS)
  //       - 8 combined image samplers (accessible from PS)
  //   - BBox Enabled
  //       - Same as standard, plus a single SSBO accessible from PS
  //   - Push Constant
  //       - Same as standard, plus 128 bytes of push constants, accessible from all stages.
  //   - Input Attachment
  //       - Same as standard, plus a single input attachment, accessible from PS.
  //
  // All three pipeline layouts use the same descriptor set layouts, but the final descriptor set
  // (SSBO) is only required when using the BBox Enabled pipeline layout.
  //
  VkDescriptorSetLayout GetDescriptorSetLayout(DESCRIPTOR_SET set) const { return m_descriptor_set_layouts[set]; }
  VkPipelineLayout GetStandardPipelineLayout() const { return m_standard_pipeline_layout; }
  VkPipelineLayout GetBBoxPipelineLayout() const { return m_bbox_pipeline_layout; }
  VkPipelineLayout GetPushConstantPipelineLayout() const { return m_push_constant_pipeline_layout; }
  VkPipelineLayout GetInputAttachmentPipelineLayout() const { return m_input_attachment_pipeline_layout; }

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

  // Accesses shader module caches
  VertexShaderCache& GetVertexShaderCache() { return m_vs_cache; }
  GeometryShaderCache& GetGeometryShaderCache() { return m_gs_cache; }
  PixelShaderCache& GetPixelShaderCache() { return m_ps_cache; }
  SharedShaderCache& GetSharedShaderCache() { return m_shared_shader_cache; }

  // Static samplers
  VkSampler GetPointSampler() const { return m_point_sampler; }
  VkSampler GetLinearSampler() const { return m_linear_sampler; }
  VkSampler GetSampler(const SamplerState& info);

  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();
  void Shutdown();

  // Finds a memory type index for the specified memory properties and the bits returned by
  // vkGetImageMemoryRequirements
  u32 GetMemoryType(u32 bits, VkMemoryPropertyFlags desired_properties);

  // Find a pipeline by the specified description, if not found, attempts to create it
  VkPipeline GetPipeline(const PipelineInfo& info);

  // Wipes out the pipeline cache, use when MSAA modes change, for example
  void ClearPipelineCache();

  // Recompile static shaders, call when MSAA mode changes, etc.
  // Destroys the old shader modules, so assumes that the pipeline cache is clear first.
  bool RecompileSharedShaders();

  // Clear sampler cache, use when anisotropy mode changes
  // WARNING: Ensure none of the objects from here are in use when calling
  void ClearSamplerCache();

private:
  bool CreateDescriptorSetLayouts();
  bool CreatePipelineLayout();
  bool CreateUtilityShaderVertexFormat();
  bool CreateStaticSamplers();

  VkInstance m_instance = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;

  VkPhysicalDeviceMemoryProperties m_device_memory_properties = {};
  VkPhysicalDeviceFeatures m_device_features = {};
  VkPhysicalDeviceLimits m_device_limits = {};

  std::array<VkDescriptorSetLayout, NUM_DESCRIPTOR_SETS> m_descriptor_set_layouts = {};
  VkDescriptorSetLayout m_input_attachment_descriptor_set_layout = VK_NULL_HANDLE;

  VkPipelineLayout m_standard_pipeline_layout = VK_NULL_HANDLE;
  VkPipelineLayout m_bbox_pipeline_layout = VK_NULL_HANDLE;
  VkPipelineLayout m_push_constant_pipeline_layout = VK_NULL_HANDLE;
  VkPipelineLayout m_input_attachment_pipeline_layout = VK_NULL_HANDLE;

  std::unique_ptr<VertexFormat> m_utility_shader_vertex_format;
  std::unique_ptr<StreamBuffer> m_utility_shader_vertex_buffer;
  std::unique_ptr<StreamBuffer> m_utility_shader_uniform_buffer;

  VertexShaderCache m_vs_cache;
  GeometryShaderCache m_gs_cache;
  PixelShaderCache m_ps_cache;

  SharedShaderCache m_shared_shader_cache;

  std::unordered_map<PipelineInfo, VkPipeline, PipelineInfoHash> m_pipeline_cache;

  VkSampler m_point_sampler = VK_NULL_HANDLE;
  VkSampler m_linear_sampler = VK_NULL_HANDLE;

  std::map<SamplerState, VkSampler> m_sampler_cache;
};

extern std::unique_ptr<ObjectCache> g_object_cache;

}  // namespace Vulkan
