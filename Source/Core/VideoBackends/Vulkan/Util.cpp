// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cassert>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"

namespace Vulkan {

namespace Util {

size_t AlignValue(size_t value, size_t alignment)
{
	// Have to use divide/multiply here in case alignment is not a power of two.
	// TODO: Can we make this assumption?
	return (value + (alignment - 1)) / alignment * alignment;
}

size_t AlignBufferOffset(size_t offset, size_t alignment)
{
	// Assume an offset of zero is already aligned to a value larger than alignment.
	if (offset == 0)
		return 0;

	return AlignValue(offset, alignment);
}

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

void BufferMemoryBarrier(VkCommandBuffer command_buffer, VkBuffer buffer, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask)
{
	VkBufferMemoryBarrier buffer_info =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,			// VkStructureType    sType
		nullptr,											// const void*        pNext
		src_access_mask,									// VkAccessFlags      srcAccessMask
		dst_access_mask,									// VkAccessFlags      dstAccessMask
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t           srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t           dstQueueFamilyIndex
		buffer,												// VkBuffer           buffer
		offset,												// VkDeviceSize       offset
		size												// VkDeviceSize       size
	};

	vkCmdPipelineBarrier(command_buffer,
						 src_stage_mask,
						 dst_stage_mask,
						 0,
						 0,
						 nullptr,
						 1,
						 &buffer_info,
						 0,
						 nullptr);
}

void ExecuteCurrentCommandsAndRestoreState(CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker)
{
	g_renderer->ResetAPIState();
	command_buffer_mgr->ExecuteCommandBuffer(false);
	state_tracker->InvalidateDescriptorSets();
	g_renderer->RestoreAPIState();
}

}		// namespace Util

UtilityShaderDraw::UtilityShaderDraw(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkRenderPass render_pass, VkShaderModule vertex_shader, VkShaderModule geometry_shader, VkShaderModule pixel_shader)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
{
	// Populate minimal pipeline state
	m_pipeline_info.vertex_format = object_cache->GetUtilityShaderVertexFormat();
	m_pipeline_info.render_pass = render_pass;
	m_pipeline_info.vs = vertex_shader;
	m_pipeline_info.gs = geometry_shader;
	m_pipeline_info.ps = pixel_shader;
	m_pipeline_info.rasterization_state.hex = Util::GetNoCullRasterizationState().hex;
	m_pipeline_info.depth_stencil_state.hex = Util::GetNoDepthTestingDepthStencilState().hex;
	m_pipeline_info.blend_state.hex = Util::GetNoBlendingBlendState().hex;
	m_pipeline_info.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

UtilityShaderVertex* UtilityShaderDraw::ReserveVertices(VkPrimitiveTopology topology, size_t count)
{
	m_pipeline_info.primitive_topology = topology;

	if (!m_object_cache->GetUtilityShaderVertexBuffer()->ReserveMemory(sizeof(UtilityShaderVertex) * count, sizeof(UtilityShaderVertex), true, true, true))
		PanicAlert("Failed to allocate space for vertices in backend shader");

	m_vertex_buffer = m_object_cache->GetUtilityShaderVertexBuffer()->GetBuffer();
	m_vertex_buffer_offset = m_object_cache->GetUtilityShaderVertexBuffer()->GetCurrentOffset();

	return reinterpret_cast<UtilityShaderVertex*>(m_object_cache->GetUtilityShaderVertexBuffer()->GetCurrentHostPointer());
}

void UtilityShaderDraw::CommitVertices(size_t count)
{
	m_object_cache->GetUtilityShaderVertexBuffer()->CommitMemory(sizeof(UtilityShaderVertex) * count);
	m_vertex_count = static_cast<uint32_t>(count);
}

void UtilityShaderDraw::UploadVertices(VkPrimitiveTopology topology, UtilityShaderVertex* vertices, size_t count)
{
	UtilityShaderVertex* upload_vertices = ReserveVertices(topology, count);
	memcpy(upload_vertices, vertices, sizeof(UtilityShaderVertex) * count);
	CommitVertices(count);
}

u8* UtilityShaderDraw::AllocateVSUniforms(size_t size)
{
	if (!m_object_cache->GetUtilityShaderUniformBuffer()->ReserveMemory(size, m_object_cache->GetUniformBufferAlignment(), true, true, true))
		PanicAlert("Failed to allocate util uniforms");

	return m_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentHostPointer();
}

void UtilityShaderDraw::CommitVSUniforms(size_t size)
{
	m_vs_uniform_buffer.buffer = m_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
	m_vs_uniform_buffer.offset = m_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentOffset();
	m_vs_uniform_buffer.range = size;

	m_object_cache->GetUtilityShaderUniformBuffer()->CommitMemory(size);
}

u8* UtilityShaderDraw::AllocatePSUniforms(size_t size)
{
	if (!m_object_cache->GetUtilityShaderUniformBuffer()->ReserveMemory(size, m_object_cache->GetUniformBufferAlignment(), true, true, true))
		PanicAlert("Failed to allocate util uniforms");

	return m_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentHostPointer();
}

void UtilityShaderDraw::CommitPSUniforms(size_t size)
{
	m_ps_uniform_buffer.buffer = m_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
	m_ps_uniform_buffer.offset = m_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentOffset();
	m_ps_uniform_buffer.range = size;

	m_object_cache->GetUtilityShaderUniformBuffer()->CommitMemory(size);
}

void UtilityShaderDraw::SetPSSampler(size_t index, VkImageView view, VkSampler sampler)
{
	m_ps_samplers[index].sampler = sampler;
	m_ps_samplers[index].imageView = view;
	m_ps_samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void UtilityShaderDraw::SetRasterizationState(const RasterizationState& state)
{
	m_pipeline_info.rasterization_state.hex = state.hex;
}

void UtilityShaderDraw::SetDepthStencilState(const DepthStencilState& state)
{
	m_pipeline_info.depth_stencil_state.hex = state.hex;
}

void UtilityShaderDraw::SetBlendState(const BlendState& state)
{
	m_pipeline_info.blend_state.hex = state.hex;
}

void UtilityShaderDraw::BeginRenderPass(VkFramebuffer framebuffer, const VkRect2D& region, const VkClearValue* clear_value)
{
	VkRenderPassBeginInfo begin_info =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		nullptr,
		m_pipeline_info.render_pass,
		framebuffer,
		region,
        (clear_value) ? 1u : 0u,
        clear_value
	};

	vkCmdBeginRenderPass(
		m_command_buffer_mgr->GetCurrentCommandBuffer(),
		&begin_info,
		VK_SUBPASS_CONTENTS_INLINE);
}

void UtilityShaderDraw::EndRenderPass()
{
	vkCmdEndRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer());
}

void UtilityShaderDraw::Draw()
{
	VkCommandBuffer command_buffer = m_command_buffer_mgr->GetCurrentCommandBuffer();

	BindVertexBuffer(command_buffer);
	BindDescriptors(command_buffer);
	if (!BindPipeline(command_buffer))
		return;

	vkCmdDraw(command_buffer, m_vertex_count, 1, 0, 0);
}

void UtilityShaderDraw::DrawQuad(int x, int y, int width, int height)
{
	UtilityShaderVertex vertices[4];
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

void UtilityShaderDraw::DrawQuad(int src_x, int src_y, int src_width, int src_height, int src_full_width, int src_full_height, int dst_x, int dst_y, int dst_width, int dst_height)
{
	float u0 = float(src_x) / float(src_full_width);
	float v0 = float(src_y) / float(src_full_height);
	float u1 = float(src_x + src_width) / float(src_full_width);
	float v1 = float(src_y + src_height) / float(src_full_height);

	UtilityShaderVertex vertices[4];
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

void UtilityShaderDraw::DrawColoredQuad(int x, int y, int width, int height, float r, float g, float b, float a)
{
	UtilityShaderVertex vertices[4];
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

void UtilityShaderDraw::DrawWithoutVertexBuffer(VkPrimitiveTopology primitive_topology, u32 vertex_count)
{
	VkCommandBuffer command_buffer = m_command_buffer_mgr->GetCurrentCommandBuffer();

	m_pipeline_info.primitive_topology = primitive_topology;

	BindDescriptors(command_buffer);
	if (!BindPipeline(command_buffer))
		return;

	vkCmdDraw(command_buffer, vertex_count, 1, 0, 0);
}

void UtilityShaderDraw::BindVertexBuffer(VkCommandBuffer command_buffer)
{
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);
}

void UtilityShaderDraw::BindDescriptors(VkCommandBuffer command_buffer)
{
	std::array<VkDescriptorSet, NUM_DESCRIPTOR_SETS> bind_descriptor_sets = {};
	std::array<uint32_t, NUM_UBO_DESCRIPTOR_SET_BINDINGS> dynamic_offsets = {};

	// uniform buffers
	{
		VkDescriptorSet set = m_command_buffer_mgr->AllocateDescriptorSet(m_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_UNIFORM_BUFFERS));
		if (set == VK_NULL_HANDLE)
			PanicAlert("Failed to allocate descriptor set for utility draw");

		VkDescriptorBufferInfo unused_buffer = { m_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer(), 0, 0 };

		VkWriteDescriptorSet set_writes[NUM_UBO_DESCRIPTOR_SET_BINDINGS] =
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
				UBO_DESCRIPTOR_SET_BINDING_VS,
				0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				nullptr,
				(m_vs_uniform_buffer.buffer != VK_NULL_HANDLE) ? &m_vs_uniform_buffer : &unused_buffer,
				nullptr
			},
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
				UBO_DESCRIPTOR_SET_BINDING_GS,
				0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				nullptr,
				&unused_buffer,
				nullptr
			},
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
				UBO_DESCRIPTOR_SET_BINDING_PS,
				0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				nullptr,
				(m_ps_uniform_buffer.buffer != VK_NULL_HANDLE) ? &m_ps_uniform_buffer : &unused_buffer,
				nullptr
			}
		};

		// We use a slow path here and just bind dynamic offsets of zero, since we have to update the binding anyway.
		vkUpdateDescriptorSets(m_object_cache->GetDevice(), NUM_UBO_DESCRIPTOR_SET_BINDINGS, set_writes, 0, nullptr);
		bind_descriptor_sets[DESCRIPTOR_SET_UNIFORM_BUFFERS] = set;
	}

	// PS samplers
	size_t first_active_sampler;
	for (first_active_sampler = 0; first_active_sampler < NUM_PIXEL_SHADER_SAMPLERS; first_active_sampler++)
	{
		if (m_ps_samplers[first_active_sampler].imageView != VK_NULL_HANDLE &&
			m_ps_samplers[first_active_sampler].sampler != VK_NULL_HANDLE)
		{
			break;
		}
	}

	// Check if we have any at all, skip the binding process entirely if we don't
	//if (first_active_sampler != NUM_PIXEL_SHADER_SAMPLERS)
	{
		std::array<VkWriteDescriptorSet, NUM_PIXEL_SHADER_SAMPLERS> set_writes;
		uint32_t num_set_writes = 0;

		// Allocate a new descriptor set
		VkDescriptorSet set = m_command_buffer_mgr->AllocateDescriptorSet(m_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_PIXEL_SHADER_SAMPLERS));
		if (set == VK_NULL_HANDLE)
			PanicAlert("Failed to allocate descriptor set for utility draw");

		// Initialize descriptor count to zero for the first image group.
		// The remaining fields will be initialized in the "create new group" branch below.
		set_writes[num_set_writes].descriptorCount = 0;

		// See Vulkan spec regarding descriptor copies, we can pack multiple bindings together in one update.
		for (size_t i = 0; i < NUM_PIXEL_SHADER_SAMPLERS; i++)
		{
			// Still batching together textures without any gaps?
			const VkDescriptorImageInfo& info = m_ps_samplers[i];
			if (info.imageView != VK_NULL_HANDLE && info.sampler != VK_NULL_HANDLE)
			{
				// Allocated a group yet?
				if (set_writes[num_set_writes].descriptorCount > 0)
				{
					// Add to the group.
					set_writes[num_set_writes].descriptorCount++;
				}
				else
				{
					// Create a new group.
					set_writes[num_set_writes] = {
						VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set,
						static_cast<uint32_t>(i), 0,
						1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						&info, nullptr, nullptr
					};
				}
			}
			else
			{
				// We've found an unbound sampler. If there is a non-zero number of images in the group,
				// and remaining images will have to be split into a separate group.
				if (set_writes[num_set_writes].descriptorCount > 0)
				{
					num_set_writes++;
					set_writes[num_set_writes].descriptorCount = 0;
				}
			}
		}

		// Complete the image group if one has been started.
		if (set_writes[num_set_writes].descriptorCount > 0)
			num_set_writes++;

		vkUpdateDescriptorSets(m_object_cache->GetDevice(), num_set_writes, set_writes.data(), 0, nullptr);
		bind_descriptor_sets[DESCRIPTOR_SET_PIXEL_SHADER_SAMPLERS] = set;
	}

	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							m_object_cache->GetPipelineLayout(),
							0,
							NUM_DESCRIPTOR_SETS,
							bind_descriptor_sets.data(),
							NUM_UBO_DESCRIPTOR_SET_BINDINGS,
							dynamic_offsets.data());
}

bool UtilityShaderDraw::BindPipeline(VkCommandBuffer command_buffer)
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
