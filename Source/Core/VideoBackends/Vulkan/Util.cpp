#include <cassert>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"

namespace Vulkan {

namespace Util {

RasterizationState GetNoCullRasterizationState()
{
	RasterizationState state = {};
	state.cull_mode = VK_CULL_MODE_NONE;
	return state;
}

DepthStencilState GetNoDepthTestingDepthStencilState()
{
	DepthStencilState state = {};
	state.test_enable = VK_FALSE;
	state.write_enable = VK_FALSE;
	state.compare_op = VK_COMPARE_OP_ALWAYS;
	return state;
}

BlendState GetNoBlendingBlendState()
{
	BlendState state = {};
	state.blend_enable = VK_FALSE;
	state.blend_op = VK_BLEND_OP_ADD;
	state.write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	state.src_blend = VK_BLEND_FACTOR_ONE;
	state.dst_blend = VK_BLEND_FACTOR_ZERO;
	state.use_dst_alpha = VK_FALSE;
	return state;
}

void SetViewportAndScissor(VkCommandBuffer command_buffer, int x, int y, int width, int height, float min_depth /*= 0.0f*/, float max_depth /*= 1.0f*/)
{
	VkViewport viewport =
	{
		static_cast<float>(x),
		static_cast<float>(y),
		static_cast<float>(width),
		static_cast<float>(height),
		min_depth,
		max_depth
	};

	VkRect2D scissor =
	{
		{ x, y },
		{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) }
	};

	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

}		// namespace Util

BackendShaderDraw::BackendShaderDraw(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkRenderPass render_pass, VkShaderModule vertex_shader, VkShaderModule geometry_shader, VkShaderModule pixel_shader)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
{
	// Populate minimal pipeline state
	m_pipeline_info.vertex_format = object_cache->GetBackendShaderVertexFormat();
	m_pipeline_info.render_pass = render_pass;
	m_pipeline_info.vs = vertex_shader;
	m_pipeline_info.gs = geometry_shader;
	m_pipeline_info.ps = pixel_shader;
	m_pipeline_info.rasterization_state.hex = Util::GetNoCullRasterizationState().hex;
	m_pipeline_info.depth_stencil_state.hex = Util::GetNoDepthTestingDepthStencilState().hex;
	m_pipeline_info.blend_state.hex = Util::GetNoBlendingBlendState().hex;
	m_pipeline_info.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

BackendShaderVertex* BackendShaderDraw::ReserveVertices(VkPrimitiveTopology topology, size_t count)
{
	m_pipeline_info.primitive_topology = topology;

	if (!m_object_cache->GetBackendShaderVertexBuffer()->ReserveMemory(sizeof(BackendShaderVertex) * count, sizeof(BackendShaderVertex), true))
		PanicAlert("Failed to allocate space for vertices in backend shader");

	m_vertex_buffer = m_object_cache->GetBackendShaderVertexBuffer()->GetBuffer();
	m_vertex_buffer_offset = m_object_cache->GetBackendShaderVertexBuffer()->GetCurrentOffset();

	return reinterpret_cast<BackendShaderVertex*>(m_object_cache->GetBackendShaderVertexBuffer()->GetCurrentHostPointer());
}

void BackendShaderDraw::CommitVertices(size_t count)
{
	m_object_cache->GetBackendShaderVertexBuffer()->CommitMemory(sizeof(BackendShaderVertex) * count);
	m_vertex_count = static_cast<uint32_t>(count);
}

void BackendShaderDraw::UploadVertices(VkPrimitiveTopology topology, BackendShaderVertex* vertices, size_t count)
{
	BackendShaderVertex* upload_vertices = ReserveVertices(topology, count);
	memcpy(upload_vertices, vertices, sizeof(BackendShaderVertex) * count);
	CommitVertices(count);
}

u8* BackendShaderDraw::AllocateVSUniforms(size_t size)
{
	// UBO alignment???
	if (!m_object_cache->GetBackendShaderUniformBuffer()->ReserveMemory(size, 65536, true))
		PanicAlert("Failed to allocate util uniforms");

	m_vs_uniform_buffer = m_object_cache->GetBackendShaderUniformBuffer()->GetBuffer();
	m_vs_uniform_buffer_offset = m_object_cache->GetBackendShaderUniformBuffer()->GetCurrentOffset();
	m_vs_uniform_buffer_size = size;

	return m_object_cache->GetBackendShaderUniformBuffer()->GetCurrentHostPointer();
}

u8* BackendShaderDraw::AllocatePSUniforms(size_t size)
{
	// UBO alignment???
	if (!m_object_cache->GetBackendShaderUniformBuffer()->ReserveMemory(size, 65536, true))
		PanicAlert("Failed to allocate util uniforms");

	m_ps_uniform_buffer = m_object_cache->GetBackendShaderUniformBuffer()->GetBuffer();
	m_ps_uniform_buffer_offset = m_object_cache->GetBackendShaderUniformBuffer()->GetCurrentOffset();
	m_ps_uniform_buffer_size = size;

	return m_object_cache->GetBackendShaderUniformBuffer()->GetCurrentHostPointer();
}

void BackendShaderDraw::SetPSSampler(size_t index, VkImageView view, VkSampler sampler)
{
	m_ps_textures[index] = view;
	m_ps_samplers[index] = sampler;
}

void BackendShaderDraw::SetRasterizationState(const RasterizationState& state)
{
	m_pipeline_info.rasterization_state.hex = state.hex;
}

void BackendShaderDraw::SetDepthStencilState(const DepthStencilState& state)
{
	m_pipeline_info.depth_stencil_state.hex = state.hex;
}

void BackendShaderDraw::SetBlendState(const BlendState& state)
{
	m_pipeline_info.blend_state.hex = state.hex;
}

void BackendShaderDraw::Draw()
{
	VkCommandBuffer command_buffer = m_command_buffer_mgr->GetCurrentCommandBuffer();

	BindVertexBuffer(command_buffer);
	BindDescriptors(command_buffer);
	if (!BindPipeline(command_buffer))
		return;

	vkCmdDraw(command_buffer, m_vertex_count, 1, 0, 0);
}

void BackendShaderDraw::DrawQuad(int x, int y, int width, int height)
{
	BackendShaderVertex vertices[4];
	vertices[0].SetPosition(-1.0f, 1.0f);
	vertices[0].SetTextureCoordinates(0.0f, 1.0f);
	vertices[0].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	vertices[1].SetPosition(1.0f, 1.0f);
	vertices[1].SetTextureCoordinates(1.0f, 1.0f);
	vertices[1].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	vertices[2].SetPosition(-1.0f, -1.0f);
	vertices[2].SetTextureCoordinates(0.0f, 0.0f);
	vertices[2].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	vertices[3].SetPosition(1.0f, -1.0f);
	vertices[3].SetTextureCoordinates(1.0f, 0.0f);
	vertices[3].SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	Util::SetViewportAndScissor(m_command_buffer_mgr->GetCurrentCommandBuffer(), x, y, width, height);
	UploadVertices(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices, ARRAYSIZE(vertices));
	Draw();
}

void BackendShaderDraw::DrawQuad(int src_x, int src_y, int src_width, int src_height, int src_full_width, int src_full_height, int dst_x, int dst_y, int dst_width, int dst_height)
{
	float u0 = float(src_x) / float(src_full_width);
	float v0 = float(src_y) / float(src_full_height);
	float u1 = float(src_x + src_width) / float(src_full_width);
	float v1 = float(src_y + src_height) / float(src_full_height);

	BackendShaderVertex vertices[4];
	vertices[0].SetPosition(-1.0f, 1.0f);
	vertices[0].SetTextureCoordinates(u0, v1);
	vertices[0].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	vertices[1].SetPosition(1.0f, 1.0f);
	vertices[1].SetTextureCoordinates(u1, v1);
	vertices[1].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	vertices[2].SetPosition(-1.0f, -1.0f);
	vertices[2].SetTextureCoordinates(u0, v0);
	vertices[2].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	vertices[3].SetPosition(1.0f, -1.0f);
	vertices[3].SetTextureCoordinates(u1, v0);
	vertices[3].SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	Util::SetViewportAndScissor(m_command_buffer_mgr->GetCurrentCommandBuffer(), dst_x, dst_y, dst_width, dst_height);
	UploadVertices(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices, ARRAYSIZE(vertices));
	Draw();
}

void BackendShaderDraw::DrawColoredQuad(int x, int y, int width, int height, float r, float g, float b, float a)
{
	BackendShaderVertex vertices[4];
	vertices[0].SetPosition(-1.0f, 1.0f);
	vertices[0].SetTextureCoordinates(0.0f, 1.0f);
	vertices[0].SetColor(r, g, b, a);
	vertices[1].SetPosition(1.0f, 1.0f);
	vertices[1].SetTextureCoordinates(1.0f, 1.0f);
	vertices[1].SetColor(r, g, b, a);
	vertices[2].SetPosition(-1.0f, -1.0f);
	vertices[2].SetTextureCoordinates(0.0f, 0.0f);
	vertices[2].SetColor(r, g, b, a);
	vertices[3].SetPosition(1.0f, -1.0f);
	vertices[3].SetTextureCoordinates(1.0f, 0.0f);
	vertices[3].SetColor(r, g, b, a);

	Util::SetViewportAndScissor(m_command_buffer_mgr->GetCurrentCommandBuffer(), x, y, width, height);
	UploadVertices(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices, ARRAYSIZE(vertices));
	Draw();
}

void BackendShaderDraw::BindVertexBuffer(VkCommandBuffer command_buffer)
{
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);
}

void BackendShaderDraw::BindDescriptors(VkCommandBuffer command_buffer)
{
	// Check if we need to bind any descriptors at all.
	size_t first_active_sampler = 0;
	for (; first_active_sampler < NUM_PIXEL_SHADER_SAMPLERS; first_active_sampler++)
	{
		if (m_ps_textures[first_active_sampler] != VK_NULL_HANDLE)
			break;
	}
	if (m_vs_uniform_buffer == VK_NULL_HANDLE && m_ps_uniform_buffer == VK_NULL_HANDLE && first_active_sampler == NUM_PIXEL_SHADER_SAMPLERS)
	{
		// SKip allocating and binding a descriptor set, since it won't be used
		return;
	}

	// Grab a descriptor set
	VkDescriptorSet set = m_command_buffer_mgr->AllocateDescriptorSet(m_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_COMBINED));
	if (set == VK_NULL_HANDLE)
		PanicAlert("Failed to allocate descriptor set for backend draw");

	// ubo descriptors
	// TODO: Should we just mirror these structs in the class?
	VkDescriptorBufferInfo vs_ubo_info = { m_vs_uniform_buffer, m_vs_uniform_buffer_offset, m_vs_uniform_buffer_size };
	VkDescriptorBufferInfo ps_ubo_info = { m_ps_uniform_buffer, m_ps_uniform_buffer_offset, m_ps_uniform_buffer_size };

	// Populate the whole thing pretty much
	std::array<VkWriteDescriptorSet, 2 + NUM_PIXEL_SHADER_SAMPLERS> write_descriptors;
	uint32_t num_write_descriptors = 0;
	if (vs_ubo_info.buffer != VK_NULL_HANDLE)
	{
		write_descriptors[num_write_descriptors++] = {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
			COMBINED_DESCRIPTOR_SET_BINDING_VS_UBO,
			0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			nullptr, &vs_ubo_info, nullptr
		};
	}
	if (ps_ubo_info.buffer != VK_NULL_HANDLE)
	{
		write_descriptors[num_write_descriptors++] = {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
			COMBINED_DESCRIPTOR_SET_BINDING_PS_UBO,
			0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			nullptr, &ps_ubo_info, nullptr
		};
	}
	if (first_active_sampler != NUM_PIXEL_SHADER_SAMPLERS)
	{
		// Set up the template descriptor, so we don't have to write the whole thing each time
		VkWriteDescriptorSet template_write =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
			COMBINED_DESCRIPTOR_SET_BINDING_PS_SAMPLERS,
			0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			nullptr, nullptr, nullptr
		};

		// Fill the first descriptor
		VkDescriptorImageInfo ps_samplers_info[NUM_PIXEL_SHADER_SAMPLERS];
		uint32_t num_image_info = 0;
		write_descriptors[num_write_descriptors] = template_write;
		write_descriptors[num_write_descriptors].pImageInfo = &ps_samplers_info[num_image_info];

		// We can't bind any null images, apparently. So if the binding is non-contiguous, we need to create multiple update entries.
		for (size_t i = 0; i < NUM_PIXEL_SHADER_SAMPLERS; i++)
		{
			// This slot has an image?
			if (m_ps_textures[i] != VK_NULL_HANDLE && m_ps_samplers[i] != VK_NULL_HANDLE)
			{
				// Add to the descriptors in this set
				ps_samplers_info[num_image_info].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ps_samplers_info[num_image_info].imageView = m_ps_textures[i];
				ps_samplers_info[num_image_info].sampler = m_ps_samplers[i];
				num_image_info++;
				write_descriptors[num_write_descriptors].descriptorCount++;
			}
			else
			{
				// Are there images currently listed?
				if (write_descriptors[num_write_descriptors].descriptorCount > 0)
				{
					// Complete the descriptor, and allocate the next one
					num_write_descriptors++;
					write_descriptors[num_write_descriptors] = template_write;
					write_descriptors[num_write_descriptors].pImageInfo = &ps_samplers_info[num_image_info];
					write_descriptors[num_write_descriptors].dstArrayElement = static_cast<uint32_t>(i);
				}
			}
		}

		// Complete the last descriptor (if any)
		if (write_descriptors[num_write_descriptors].descriptorCount > 0)
			num_write_descriptors++;
	}

	assert(num_write_descriptors > 0);
	vkUpdateDescriptorSets(m_object_cache->GetDevice(), num_write_descriptors, write_descriptors.data(), 0, nullptr);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_object_cache->GetPipelineLayout(), 0, 1, &set, 0, nullptr);
}

bool BackendShaderDraw::BindPipeline(VkCommandBuffer command_buffer)
{
	VkPipeline pipeline = m_object_cache->GetPipeline(m_pipeline_info);
	if (pipeline == VK_NULL_HANDLE)
	{
		PanicAlert("Failed to get pipeline for backend shader draw");
		return false;
	}

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	return true;
}

}		// namespace Vulkan
