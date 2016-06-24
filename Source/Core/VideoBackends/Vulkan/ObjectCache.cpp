// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/VertexFormat.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"

namespace Vulkan {

ObjectCache::ObjectCache(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device)
	: m_instance(instance)
	, m_physical_device(physical_device)
	, m_device(device)
	, m_vs_cache(device)
	, m_gs_cache(device)
	, m_fs_cache(device)
{
	// Read device physical memory properties, we need it for allocating buffers
	vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_memory_properties);
}

ObjectCache::~ObjectCache()
{
	ClearPipelineCache();
}

bool ObjectCache::Initialize()
{
	if (!CreateDescriptorSetLayouts())
		return false;

	if (!CreatePipelineLayout())
		return false;

	return true;
}

u32 ObjectCache::GetMemoryType(u32 bits, VkMemoryPropertyFlags desired_properties)
{
	for (u32 i = 0; i < 32; i++)
	{
		if ((bits & (1 << i)) != 0 && (m_device_memory_properties.memoryTypes[i].propertyFlags & desired_properties) == desired_properties)
			return i;
	}

	PanicAlert("Unable to find memory type for %x:%x", bits, desired_properties);
	return 0;
}

static VkPipelineRasterizationStateCreateInfo GetVulkanRasterizationState(const RasterizationState& state)
{
	return {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,									// VkPipelineRasterizationStateCreateFlags    flags
		VK_TRUE,							// VkBool32                                   depthClampEnable
		VK_TRUE,							// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,				// VkPolygonMode                              polygonMode
		state.cull_mode,					// VkCullModeFlags                            cullMode
		VK_FRONT_FACE_CLOCKWISE,			// VkFrontFace                                frontFace
		VK_FALSE,							// VkBool32                                   depthBiasEnable
		0.0f,                               // float                                      depthBiasConstantFactor
		0.0f,                               // float                                      depthBiasClamp
		0.0f,                               // float                                      depthBiasSlopeFactor
		1.0f                                // float                                      lineWidth
	};
}

static VkPipelineDepthStencilStateCreateInfo GetVulkanDepthStencilState(const DepthStencilState& state)
{
	return {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,									// VkPipelineDepthStencilStateCreateFlags    flags;
		state.test_enable,					// VkBool32                                  depthTestEnable
		state.write_enable,					// VkBool32                                  depthWriteEnable
		state.compare_op,					// VkCompareOp                               depthCompareOp
		VK_TRUE,							// VkBool32                                  depthBoundsTestEnable
		VK_FALSE,							// VkBool32                                  stencilTestEnable
		{},									// VkStencilOpState                          front
		{},									// VkStencilOpState                          back
		0.0f,								// float                                     minDepthBounds
		1.0f								// float                                     maxDepthBounds
	};
}

static VkPipelineColorBlendAttachmentState GetVulkanAttachmentBlendState(const BlendState& state)
{
	return {
		state.blend_enable,
		state.src_blend,
		state.dst_blend,
		state.blend_op,
		state.src_blend,
		state.dst_blend,
		state.blend_op,
		state.write_mask
	};
}

static VkPipelineColorBlendStateCreateInfo GetVulkanColorBlendState(const BlendState& state, const VkPipelineColorBlendAttachmentState* attachments, uint32_t num_attachments)
{
	// TODO: Logic ops
	return {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,																	// VkPipelineColorBlendStateCreateFlags          flags
		VK_FALSE,															// VkBool32                                      logicOpEnable
		VK_LOGIC_OP_CLEAR,													// VkLogicOp                                     logicOp
		num_attachments,													// uint32_t                                      attachmentCount
		attachments,														// const VkPipelineColorBlendAttachmentState*    pAttachments
		{ 1.0f, 1.0f, 1.0f, 1.0f }											// float                                         blendConstants[4]
	};
}

VkPipeline ObjectCache::GetPipeline(const PipelineInfo& info)
{
	// TODO: Pipeline caches
	auto iter = m_pipeline_cache.find(info);
	if (iter != m_pipeline_cache.end())
		return iter->second;

	// Declare descriptors for empty vertex buffers/attributes
	static const VkPipelineVertexInputStateCreateInfo empty_vertex_input_state = 
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,			// VkStructureType                             sType
		nullptr,															// const void*                                pNext
		0,																	// VkPipelineVertexInputStateCreateFlags       flags
		0,																	// uint32_t                                    vertexBindingDescriptionCount
		nullptr,															// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		0,																	// uint32_t                                    vertexAttributeDescriptionCount
		nullptr																// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	// Vertex inputs
	const VkPipelineVertexInputStateCreateInfo& vertex_input_state = (info.vertex_format) ? info.vertex_format->GetVertexInputStateInfo() : empty_vertex_input_state;

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,		// VkStructureType                            sType
		nullptr,															// const void*                                pNext
		0,																	// VkPipelineInputAssemblyStateCreateFlags    flags
		info.primitive_topology,											// VkPrimitiveTopology                        topology
		VK_TRUE																// VkBool32                                   primitiveRestartEnable
	};

	// Shaders to stages
	VkPipelineShaderStageCreateInfo shader_stages[3];
	uint32_t num_shader_stages = 0;
	if (info.vs != VK_NULL_HANDLE)
		shader_stages[num_shader_stages++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, info.vs, "main" };
	if (info.vs != VK_NULL_HANDLE)
		shader_stages[num_shader_stages++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_GEOMETRY_BIT, info.gs, "main" };
	if (info.vs != VK_NULL_HANDLE)
		shader_stages[num_shader_stages++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, info.ps, "main" };

	// Fill in full descriptor structs
	VkPipelineRasterizationStateCreateInfo rasterization_state = GetVulkanRasterizationState(info.rasterization_state);
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = GetVulkanDepthStencilState(info.depth_stencil_state);
	VkPipelineColorBlendAttachmentState blend_attachment_state = GetVulkanAttachmentBlendState(info.blend_state);
	VkPipelineColorBlendStateCreateInfo blend_state = GetVulkanColorBlendState(info.blend_state, &blend_attachment_state, 1);

	// TODO: multisampling
	VkPipelineMultisampleStateCreateInfo multisample_state =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr,
		0,									// VkPipelineMultisampleStateCreateFlags    flags
		VK_SAMPLE_COUNT_1_BIT,				// VkSampleCountFlagBits                    rasterizationSamples
		VK_FALSE,							// VkBool32                                 sampleShadingEnable
		0.0f,								// float                                    minSampleShading
		nullptr,							// const VkSampleMask*                      pSampleMask;
		VK_FALSE,							// VkBool32                                 alphaToCoverageEnable
		VK_FALSE							// VkBool32                                 alphaToOneEnable
	};

	// Viewport is used with dynamic state
	static const VkViewport viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	static const VkRect2D scissor = { { 0, 0 }, { 1, 1 } };
	static const VkPipelineViewportStateCreateInfo viewport_state =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr,
		0,									// VkPipelineViewportStateCreateFlags    flags;
		1,									// uint32_t                              viewportCount
		&viewport,							// const VkViewport*                     pViewports
		1,									// uint32_t                              scissorCount
		&scissor							// const VkRect2D*                       pScissors
	};

	// Set viewport and scissor dynamic state so we can change it elsewhere
	static const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	static const VkPipelineDynamicStateCreateInfo dynamic_state =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr,
		0,									// VkPipelineDynamicStateCreateFlags    flags
		_countof(dynamic_states),			// uint32_t                             dynamicStateCount
		dynamic_states						// const VkDynamicState*                pDynamicStates
	};

	// Combine to pipeline info
	VkGraphicsPipelineCreateInfo pipeline_info =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr,
		0,									// VkPipelineCreateFlags                            flags
		num_shader_stages,					// uint32_t                                         stageCount
		shader_stages,						// const VkPipelineShaderStageCreateInfo*           pStages
		&vertex_input_state,				// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
		&input_assembly_state,				// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
		nullptr,							// const VkPipelineTessellationStateCreateInfo*     pTessellationState
		&viewport_state,					// const VkPipelineViewportStateCreateInfo*         pViewportState
		&rasterization_state,				// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
		&multisample_state,					// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
		&depth_stencil_state,				// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
		&blend_state,						// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
		&dynamic_state,						// const VkPipelineDynamicStateCreateInfo*          pDynamicState
		m_pipeline_layout,					// VkPipelineLayout                                 layout
		info.render_pass,					// VkRenderPass                                     renderPass
		0,									// uint32_t                                         subpass
		VK_NULL_HANDLE,						// VkPipeline                                       basePipelineHandle
		-1									// int32_t                                          basePipelineIndex
	};
	
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult res = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines failed: ");

	m_pipeline_cache.emplace(info, pipeline);
	return pipeline;
}

void ObjectCache::ClearPipelineCache()
{
	// Care here to ensure that pipelines are destroyed before shader modules
	for (const auto& it : m_pipeline_cache)
	{
		if (it.second != VK_NULL_HANDLE)
			vkDestroyPipeline(m_device, it.second, nullptr);
	}
	m_pipeline_cache.clear();
}

bool ObjectCache::CreateDescriptorSetLayouts()
{
	VkDescriptorSetLayoutBinding combined_set_bindings[] = {
		{ 2,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				1,		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT	},
		{ 3,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				1,		VK_SHADER_STAGE_GEOMETRY_BIT								},
		{ 1,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				1,		VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 0,	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		16,		VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 4,	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				1,		VK_SHADER_STAGE_FRAGMENT_BIT								},
	};

	VkDescriptorSetLayoutCreateInfo combined_layout_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		_countof(combined_set_bindings),
		combined_set_bindings
	};

	VkResult res = vkCreateDescriptorSetLayout(m_device, &combined_layout_info, nullptr, &m_descriptor_set_layouts[DESCRIPTOR_SET_COMBINED]);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout failed: ");
		return false;
	}

	return true;
}

bool ObjectCache::CreatePipelineLayout()
{
	VkPipelineLayoutCreateInfo info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType                 sType
		nullptr,												// const void*                     pNext
		0,														// VkPipelineLayoutCreateFlags     flags
		static_cast<uint32_t>(m_descriptor_set_layouts.size()),	// uint32_t                        setLayoutCount
		m_descriptor_set_layouts.data(),						// const VkDescriptorSetLayout*    pSetLayouts
		0,														// uint32_t                        pushConstantRangeCount
		nullptr													// const VkPushConstantRange*      pPushConstantRanges
	};

	VkResult res = vkCreatePipelineLayout(m_device, &info, nullptr, &m_pipeline_layout);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreatePipelineLayout failed: ");
		return false;
	}

	return true;
}

// Comparison operators for PipelineInfos
// since these all boil down to POD types, we can just memcmp the entire thing for speed

bool operator==(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool operator!=(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) != 0;
}

bool operator<(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

bool operator>(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) > 0;
}

}
