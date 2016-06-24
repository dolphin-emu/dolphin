// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/VulkanImports.h"
#include "VideoBackends/Vulkan/ShaderCache.h"

namespace Vulkan {

// Game shader state encompassed by pipelines
struct PipelineInfo
{
	// TODO: Vertex formats
	VkShaderModule vs;
	VkShaderModule gs;
	VkShaderModule ps;
	VkRenderPass render_pass;
	RasterizationState rasterization_state;
	DepthStencilState depth_stencil_state;
	BlendState blend_state;
	VkPrimitiveTopology primitive_topology;
};

bool operator==(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator!=(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator<(const PipelineInfo& lhs, const PipelineInfo& rhs);
bool operator>(const PipelineInfo& lhs, const PipelineInfo& rhs);

class ObjectCache
{
public:
	ObjectCache(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device);
	~ObjectCache();

	VkInstance GetVulkanInstance() const { return m_instance; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
	VkDevice GetDevice() const { return m_device; }

	const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const { return m_device_memory_properties; }

	// Support bits
	bool SupportsGeometryShaders() const { return m_support_bits.geometry_shaders; }

	VkDescriptorSetLayout GetDescriptorSetLayout(DESCRIPTOR_SET set) const { return m_descriptor_set_layouts[set]; }
	VkPipelineLayout GetPipelineLayout() const { return m_pipeline_layout; }

	// Accesses shader module caches
	VertexShaderCache& GetVertexShaderCache() { return m_vs_cache; }
	GeometryShaderCache& GetGeometryShaderCache() { return m_gs_cache; }
	FragmentShaderCache& GetPixelShaderCache() { return m_fs_cache; }

	bool Initialize();

	// Finds a memory type index for the specified memory properties and the bits returned by vkGetImageMemoryRequirements
	u32 GetMemoryType(u32 bits, VkMemoryPropertyFlags desired_properties);

	// Find a pipeline by the specified description, if not found, attempts to create it
	VkPipeline GetPipeline(const PipelineInfo& info);

	// Wipes out the pipeline cache, use when MSAA modes change, for example
	void ClearPipelineCache();

private:
	bool CreateDescriptorSetLayouts();
	bool CreatePipelineLayout();

	VkInstance m_instance = VK_NULL_HANDLE;
	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;

	VkPhysicalDeviceMemoryProperties m_device_memory_properties = {};

	struct
	{
		u32 geometry_shaders : 1;
		u32 bits;
	} m_support_bits;

	std::array<VkDescriptorSetLayout, NUM_DESCRIPTOR_SETS> m_descriptor_set_layouts = {};

	VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

	VertexShaderCache m_vs_cache;
	GeometryShaderCache m_gs_cache;
	FragmentShaderCache m_fs_cache;

	// TODO: Replace with hash table
	std::map<PipelineInfo, VkPipeline> m_pipeline_cache;

};

}  // namespace Vulkan
