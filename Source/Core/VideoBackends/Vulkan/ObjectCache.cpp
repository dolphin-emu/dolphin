// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VertexFormat.h"

namespace Vulkan {

ObjectCache::ObjectCache(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, CommandBufferManager* command_buffer_mgr)
	: m_instance(instance)
	, m_physical_device(physical_device)
	, m_device(device)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_vs_cache(device)
	, m_gs_cache(device)
	, m_ps_cache(device)
	, m_static_shader_cache(device, &m_vs_cache, &m_gs_cache, &m_ps_cache)
{
	// Read device physical memory properties, we need it for allocating buffers
	vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_memory_properties);
	vkGetPhysicalDeviceFeatures(physical_device, &m_device_features);

	// Read limits, useful for buffer alignment and such.
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(physical_device, &device_properties);
	m_device_limits = device_properties.limits;

	// Would any drivers be this silly? I hope not...
	m_device_limits.minUniformBufferOffsetAlignment = std::max(m_device_limits.minUniformBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_limits.minTexelBufferOffsetAlignment = std::max(m_device_limits.minTexelBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_limits.optimalBufferCopyOffsetAlignment = std::max(m_device_limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_limits.optimalBufferCopyRowPitchAlignment = std::max(m_device_limits.optimalBufferCopyRowPitchAlignment, static_cast<VkDeviceSize>(1));
}

ObjectCache::~ObjectCache()
{
	ClearPipelineCache();
	ClearSamplerCache();

	if (m_point_sampler != VK_NULL_HANDLE)
		vkDestroySampler(m_device, m_point_sampler, nullptr);

	if (m_linear_sampler != VK_NULL_HANDLE)
		vkDestroySampler(m_device, m_linear_sampler, nullptr);

	if (m_pipeline_layout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);

	for (VkDescriptorSetLayout layout : m_descriptor_set_layouts)
	{
		if (layout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
	}
}

bool ObjectCache::Initialize()
{
	if (!CreateDescriptorSetLayouts())
		return false;

	if (!CreatePipelineLayout())
		return false;

	if (!CreateUtilityShaderVertexFormat())
		return false;

	if (!CreateStaticSamplers())
		return false;

	m_utility_shader_vertex_buffer = StreamBuffer::Create(this, m_command_buffer_mgr, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1024 * 1024, 4 * 1024 * 1024);
	m_utility_shader_uniform_buffer = StreamBuffer::Create(this, m_command_buffer_mgr, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 1024, 4 * 1024 * 1024);
	if (!m_utility_shader_vertex_buffer || !m_utility_shader_uniform_buffer)
		return false;

	if (!m_static_shader_cache.CompileShaders())
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
	return
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,			// VkStructureType                           sType
		nullptr,															// const void*                               pNext
		0,																	// VkPipelineRasterizationStateCreateFlags   flags
		VK_FALSE,															// VkBool32                                  depthClampEnable
		VK_FALSE,															// VkBool32                                  rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,												// VkPolygonMode                             polygonMode
		state.cull_mode,													// VkCullModeFlags                           cullMode
		VK_FRONT_FACE_CLOCKWISE,											// VkFrontFace                               frontFace
		VK_FALSE,															// VkBool32                                  depthBiasEnable
		0.0f,																// float                                     depthBiasConstantFactor
		0.0f,																// float                                     depthBiasClamp
		0.0f,																// float                                     depthBiasSlopeFactor
		1.0f																// float                                     lineWidth
	};
}

static VkPipelineDepthStencilStateCreateInfo GetVulkanDepthStencilState(const DepthStencilState& state)
{
	return
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,			// VkStructureType                           sType
		nullptr,															// const void*                               pNext
		0,																	// VkPipelineDepthStencilStateCreateFlags    flags
		state.test_enable,													// VkBool32                                  depthTestEnable
		state.write_enable,													// VkBool32                                  depthWriteEnable
		state.compare_op,													// VkCompareOp                               depthCompareOp
		VK_TRUE,															// VkBool32                                  depthBoundsTestEnable
		VK_FALSE,															// VkBool32                                  stencilTestEnable
		{},																	// VkStencilOpState                          front
		{},																	// VkStencilOpState                          back
		0.0f,																// float                                     minDepthBounds
		1.0f																// float                                     maxDepthBounds
	};
}

static VkPipelineColorBlendAttachmentState GetVulkanAttachmentBlendState(const BlendState& state)
{
	VkPipelineColorBlendAttachmentState vk_state =
	{
		state.blend_enable,													// VkBool32                                  blendEnable
		state.src_blend,													// VkBlendFactor                             srcColorBlendFactor
		state.dst_blend,													// VkBlendFactor                             dstColorBlendFactor
		state.blend_op,														// VkBlendOp                                 colorBlendOp
		state.src_blend,													// VkBlendFactor                             srcAlphaBlendFactor
		state.dst_blend,													// VkBlendFactor                             dstAlphaBlendFactor
		state.blend_op,														// VkBlendOp                                 alphaBlendOp
		state.write_mask													// VkColorComponentFlags                     colorWriteMask
	};

	switch (vk_state.srcAlphaBlendFactor)
	{
	case VK_BLEND_FACTOR_SRC_COLOR:				vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;				break;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:	vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		break;
	case VK_BLEND_FACTOR_DST_COLOR:				vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;				break;
	case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:	vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;		break;
	default:																											break;
	}

	switch (vk_state.dstAlphaBlendFactor)
	{
	case VK_BLEND_FACTOR_SRC_COLOR:				vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;				break;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:	vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		break;
	case VK_BLEND_FACTOR_DST_COLOR:				vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;				break;
	case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:	vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;		break;
	default:																											break;
	}

	if (state.use_dst_alpha)
	{
		// Colors should blend against SRC1_ALPHA
		if (vk_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA)
			vk_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
		else if (vk_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
			vk_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

		// Colors should blend against SRC1_ALPHA
		if (vk_state.dstColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA)
			vk_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
		else if (vk_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
			vk_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

		vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		vk_state.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	return vk_state;
}

static VkPipelineColorBlendStateCreateInfo GetVulkanColorBlendState(const BlendState& state, const VkPipelineColorBlendAttachmentState* attachments, uint32_t num_attachments)
{
	VkPipelineColorBlendStateCreateInfo vk_state =
  {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,   // VkStructureType                               sType
		nullptr,                                                    // const void*                                   pNext
		0,                                                          // VkPipelineColorBlendStateCreateFlags          flags
		state.logic_op_enable,                                      // VkBool32                                      logicOpEnable
		state.logic_op,                                             // VkLogicOp                                     logicOp
		num_attachments,                                            // uint32_t                                      attachmentCount
		attachments,                                                // const VkPipelineColorBlendAttachmentState*    pAttachments
    { 1.0f, 1.0f, 1.0f, 1.0f }                                  // float                                         blendConstants[4]
	};

  return vk_state;
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
	if (info.gs != VK_NULL_HANDLE)
		shader_stages[num_shader_stages++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_GEOMETRY_BIT, info.gs, "main" };
	if (info.ps != VK_NULL_HANDLE)
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
		1.0f,								// float                                    minSampleShading
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
		ARRAYSIZE(dynamic_states),			// uint32_t                             dynamicStateCount
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
		info.pipeline_layout,					// VkPipelineLayout                                 layout
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

bool ObjectCache::RecompileStaticShaders()
{
	m_static_shader_cache.DestroyShaders();
	if (!m_static_shader_cache.CompileShaders())
	{
		PanicAlert("Failed to recompile static shaders.");
		return false;
	}

	return true;
}

void ObjectCache::ClearSamplerCache()
{
	for (const auto& it : m_sampler_cache)
	{
		if (it.second != VK_NULL_HANDLE)
			vkDestroySampler(m_device, it.second, nullptr);
	}
	m_sampler_cache.clear();
}

bool ObjectCache::CreateDescriptorSetLayouts()
{
	static const VkDescriptorSetLayoutBinding ubo_set_bindings[] =
	{
		{ UBO_DESCRIPTOR_SET_BINDING_VS,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,	1,	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT	},
		{ UBO_DESCRIPTOR_SET_BINDING_GS,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,	1,	VK_SHADER_STAGE_GEOMETRY_BIT								},
		{ UBO_DESCRIPTOR_SET_BINDING_PS,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								}
	};

	// Annoying these have to be split, apparently we can't partially update an array without the debug layer crying.
	static const VkDescriptorSetLayoutBinding sampler_set_bindings[] =
	{
		{ 0,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 1,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 2,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 3,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 4,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 5,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 6,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								},
		{ 7,													VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1,	VK_SHADER_STAGE_FRAGMENT_BIT								}
	};

//	static const VkDescriptorSetLayoutBinding ssbo_set_bindings[] =
//	{
//		{ 0,													VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			1,	VK_SHADER_STAGE_FRAGMENT_BIT								}
//	};

	static const VkDescriptorSetLayoutCreateInfo create_infos[NUM_DESCRIPTOR_SETS] =
	{
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
			0, ARRAYSIZE(ubo_set_bindings), ubo_set_bindings
		},
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
			0, ARRAYSIZE(sampler_set_bindings), sampler_set_bindings
		},
// 		{
// 			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
// 			0, ARRAYSIZE(ssbo_set_bindings), ssbo_set_bindings
// 		}
	};

	for (size_t i = 0; i < NUM_DESCRIPTOR_SETS; i++)
	{
		VkResult res = vkCreateDescriptorSetLayout(m_device, &create_infos[i], nullptr, &m_descriptor_set_layouts[i]);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout failed: ");
			return false;
		}
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

bool ObjectCache::CreateUtilityShaderVertexFormat()
{
	PortableVertexDeclaration vtx_decl = {};
	vtx_decl.position.enable = true;
	vtx_decl.position.type = VAR_FLOAT;
	vtx_decl.position.components = 4;
	vtx_decl.position.integer = false;
	vtx_decl.position.offset = offsetof(UtilityShaderVertex, Position);
	vtx_decl.texcoords[0].enable = true;
	vtx_decl.texcoords[0].type = VAR_FLOAT;
	vtx_decl.texcoords[0].components = 4;
	vtx_decl.texcoords[0].integer = false;
	vtx_decl.texcoords[0].offset = offsetof(UtilityShaderVertex, TexCoord);
	vtx_decl.colors[0].enable = true;
	vtx_decl.colors[0].type = VAR_FLOAT;
	vtx_decl.colors[0].components = 4;
	vtx_decl.colors[0].integer = false;
	vtx_decl.colors[0].offset = offsetof(UtilityShaderVertex, Color);
	vtx_decl.stride = sizeof(UtilityShaderVertex);

	m_utility_shader_vertex_format = std::make_unique<VertexFormat>(vtx_decl);
	return true;
}

bool ObjectCache::CreateStaticSamplers()
{
	VkSamplerCreateInfo create_info =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType         sType
		nullptr,									// const void*             pNext
		0,											// VkSamplerCreateFlags    flags
		VK_FILTER_NEAREST,							// VkFilter                magFilter
		VK_FILTER_NEAREST,							// VkFilter                minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode     mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode    addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode    addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode    addressModeW
		0.0f,										// float                   mipLodBias
		VK_FALSE,									// VkBool32                anisotropyEnable
		1.0f,										// float                   maxAnisotropy
		VK_FALSE,									// VkBool32                compareEnable
		VK_COMPARE_OP_ALWAYS,						// VkCompareOp             compareOp
		std::numeric_limits<float>::min(),			// float                   minLod
		std::numeric_limits<float>::max(),			// float                   maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor           borderColor
		VK_FALSE									// VkBool32                unnormalizedCoordinates
	};

	VkResult res = vkCreateSampler(m_device, &create_info, nullptr, &m_point_sampler);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");
		return false;
	}

	// change for linear
	create_info.minFilter = VK_FILTER_LINEAR;
	create_info.magFilter = VK_FILTER_LINEAR;
	create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	res = vkCreateSampler(m_device, &create_info, nullptr, &m_linear_sampler);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");
		return false;
	}

	return true;
}

VkSampler ObjectCache::GetSampler(const SamplerState& info)
{
	auto iter = m_sampler_cache.find(info);
	if (iter != m_sampler_cache.end())
		return iter->second;

	VkSamplerCreateInfo create_info =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType         sType
		nullptr,									// const void*             pNext
		0,											// VkSamplerCreateFlags    flags
		info.mag_filter,							// VkFilter                magFilter
		info.min_filter,							// VkFilter                minFilter
		info.mipmap_mode,							// VkSamplerMipmapMode     mipmapMode
		info.wrap_u,								// VkSamplerAddressMode    addressModeU
		info.wrap_v,								// VkSamplerAddressMode    addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode    addressModeW
		static_cast<float>(info.lod_bias.Value()),	// float                   mipLodBias
		VK_FALSE,									// VkBool32                anisotropyEnable
		1.0f,										// float                   maxAnisotropy
		VK_FALSE,									// VkBool32                compareEnable
		VK_COMPARE_OP_ALWAYS,						// VkCompareOp             compareOp
		static_cast<float>(info.min_lod.Value()),	// float                   minLod
		static_cast<float>(info.max_lod.Value()),	// float                   maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor           borderColor
		VK_FALSE									// VkBool32                unnormalizedCoordinates
	};

	// TODO: Anisotropy

	VkSampler sampler = VK_NULL_HANDLE;
	VkResult res = vkCreateSampler(m_device, &create_info, nullptr, &sampler);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");

	// Store it even if it failed
	m_sampler_cache.emplace(info, sampler);
	return sampler;
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

bool operator==(const SamplerState& lhs, const SamplerState& rhs)
{
	return lhs.hex == rhs.hex;
}

bool operator!=(const SamplerState& lhs, const SamplerState& rhs)
{
	return lhs.hex != rhs.hex;
}

bool operator>(const SamplerState& lhs, const SamplerState& rhs)
{
	return lhs.hex > rhs.hex;
}

bool operator<(const SamplerState& lhs, const SamplerState& rhs)
{
	return lhs.hex < rhs.hex;
}

}
