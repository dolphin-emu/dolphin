// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"

#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan {

StateTracker::StateTracker(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
{
	// Set some sensible defaults
	m_pipeline_state.rasterization_state.cull_mode = VK_CULL_MODE_NONE;
	m_pipeline_state.depth_stencil_state.test_enable = VK_TRUE;
	m_pipeline_state.depth_stencil_state.write_enable = VK_TRUE;
	m_pipeline_state.depth_stencil_state.compare_op = VK_COMPARE_OP_LESS;
	m_pipeline_state.blend_state.blend_enable = VK_FALSE;
	m_pipeline_state.blend_state.blend_op = VK_BLEND_OP_ADD;
	m_pipeline_state.blend_state.write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pipeline_state.blend_state.src_blend = VK_BLEND_FACTOR_ONE;
	m_pipeline_state.blend_state.dst_blend = VK_BLEND_FACTOR_ZERO;
	m_pipeline_state.blend_state.use_dst_alpha = VK_FALSE;

	// Initialize all samplers to point by default
	for (size_t i = 0; i < NUM_PIXEL_SHADER_SAMPLERS; i++)
	{
		m_bindings.ps_samplers[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		m_bindings.ps_samplers[i].imageView = VK_NULL_HANDLE;
		m_bindings.ps_samplers[i].sampler = object_cache->GetPointSampler();
	}
}

StateTracker::~StateTracker()
{

}

void StateTracker::SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset)
{
	if (m_vertex_buffer == buffer &&
		m_vertex_buffer_offset == offset)
	{
		return;
	}

	m_vertex_buffer = buffer;
	m_vertex_buffer_offset = offset;
	m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
}

void StateTracker::SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type)
{
	if (m_index_buffer == buffer &&
		m_index_buffer_offset == offset &&
		m_index_type == type)
	{
		return;
	}

	m_index_buffer = buffer;
	m_index_buffer_offset = offset;
	m_index_type = type;
	m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void StateTracker::SetRenderPass(VkRenderPass render_pass)
{
	if (m_pipeline_state.render_pass == render_pass)
		return;

	m_pipeline_state.render_pass = render_pass;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetVertexFormat(const VertexFormat* vertex_format)
{
	if (m_pipeline_state.vertex_format == vertex_format)
		return;

	m_pipeline_state.vertex_format = vertex_format;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetPrimitiveTopology(VkPrimitiveTopology primitive_topology)
{
	if (m_pipeline_state.primitive_topology == primitive_topology)
		return;

	m_pipeline_state.primitive_topology = primitive_topology;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::DisableBackFaceCulling()
{
	if (m_pipeline_state.rasterization_state.cull_mode == VK_CULL_MODE_NONE)
		return;

	m_pipeline_state.rasterization_state.cull_mode = VK_CULL_MODE_NONE;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetRasterizationState(const RasterizationState& state)
{
	if (m_pipeline_state.rasterization_state.hex == state.hex)
		return;

	m_pipeline_state.rasterization_state.hex = state.hex;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetDepthStencilState(const DepthStencilState& state)
{
	if (m_pipeline_state.depth_stencil_state.hex == state.hex)
		return;

	m_pipeline_state.depth_stencil_state.hex = state.hex;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetBlendState(const BlendState& state)
{
	if (m_pipeline_state.blend_state.hex == state.hex)
		return;

	m_pipeline_state.blend_state.hex = state.hex;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetColorMask(u32 mask)
{
	if (m_pipeline_state.blend_state.write_mask == mask)
		return;

	m_pipeline_state.blend_state.write_mask = mask;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

bool StateTracker::CheckForShaderChanges(u32 gx_primitive_type, DSTALPHA_MODE dstalpha_mode)
{
	VertexShaderUid vs_uid = GetVertexShaderUid(API_VULKAN);
	GeometryShaderUid gs_uid = GetGeometryShaderUid(gx_primitive_type, API_VULKAN);
	PixelShaderUid ps_uid = GetPixelShaderUid(dstalpha_mode, API_VULKAN);

	bool changed = false;

	if (vs_uid != m_vs_uid)
	{
		m_pipeline_state.vs = m_object_cache->GetVertexShaderCache().GetShaderForUid(vs_uid, gx_primitive_type, dstalpha_mode);
		m_vs_uid = vs_uid;
		changed = true;
	}

	if (gs_uid != m_gs_uid)
	{
		if (gs_uid.GetUidData()->IsPassthrough())
			m_pipeline_state.gs = VK_NULL_HANDLE;
		else
			m_pipeline_state.gs = m_object_cache->GetGeometryShaderCache().GetShaderForUid(gs_uid, gx_primitive_type, dstalpha_mode);

		m_gs_uid = gs_uid;
		changed = true;
	}

	if (ps_uid != m_ps_uid)
	{
		m_pipeline_state.ps = m_object_cache->GetPixelShaderCache().GetShaderForUid(ps_uid, gx_primitive_type, dstalpha_mode);
		m_ps_uid = ps_uid;
		changed = true;
	}

	m_gx_primitive_type = gx_primitive_type;
	m_dstalpha_mode = dstalpha_mode;

	if (changed)
		m_dirty_flags |= DIRTY_FLAG_PIPELINE;

	return changed;
}

void StateTracker::SetVSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	if (m_bindings.vs_ubo.buffer != buffer ||
		m_bindings.vs_ubo.offset != offset ||
		m_bindings.vs_ubo.range != range)
	{
		return;
	}

	m_bindings.vs_ubo.buffer = buffer;
	m_bindings.vs_ubo.offset = offset;
	m_bindings.vs_ubo.range = range;
	m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SET | DIRTY_FLAG_VS_UBO;
}

void StateTracker::SetGSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	if (m_bindings.gs_ubo.buffer != buffer ||
		m_bindings.gs_ubo.offset != offset ||
		m_bindings.gs_ubo.range != range)
	{
		return;
	}

	m_bindings.gs_ubo.buffer = buffer;
	m_bindings.gs_ubo.offset = offset;
	m_bindings.gs_ubo.range = range;
	m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SET | DIRTY_FLAG_GS_UBO;
}

void StateTracker::SetPSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	if (m_bindings.ps_ubo.buffer != buffer ||
		m_bindings.ps_ubo.offset != offset ||
		m_bindings.ps_ubo.range != range)
	{
		return;
	}

	m_bindings.ps_ubo.buffer = buffer;
	m_bindings.ps_ubo.offset = offset;
	m_bindings.ps_ubo.range = range;
	m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SET | DIRTY_FLAG_PS_UBO;
}

void StateTracker::SetPSTexture(size_t index, VkImageView view)
{
	if (m_bindings.ps_samplers[index].imageView == view)
		return;

	m_bindings.ps_samplers[index].imageView = view;
	m_bindings.ps_samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SET | DIRTY_FLAG_PS_SAMPLERS;
}

void StateTracker::SetPSSampler(size_t index, VkSampler sampler)
{
	if (m_bindings.ps_samplers[index].sampler == sampler)
		return;

	m_bindings.ps_samplers[index].sampler = sampler;
	m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SET | DIRTY_FLAG_PS_SAMPLERS;
}

void StateTracker::InvalidateDescriptorSet()
{
	m_descriptor_set = VK_NULL_HANDLE;
	m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SET;
}

void StateTracker::SetViewport(const VkViewport& viewport)
{
	if (memcmp(&m_viewport, &viewport, sizeof(viewport)) == 0)
		return;

	m_viewport = viewport;
	m_dirty_flags |= DIRTY_FLAG_VIEWPORT;
}

void StateTracker::SetScissor(const VkRect2D& scissor)
{
	if (memcmp(&m_scissor, &scissor, sizeof(scissor)) == 0)
		return;

	m_scissor = scissor;
	m_dirty_flags |= DIRTY_FLAG_SCISSOR;
}

bool StateTracker::Bind(VkCommandBuffer command_buffer, bool rebind_all /*= false*/)
{
	// Get new pipeline object if any parts have changed
	if (m_dirty_flags & DIRTY_FLAG_PIPELINE && !UpdatePipeline())
	{
		ERROR_LOG(VIDEO, "Failed to get pipeline object, skipping draw");
		return false;
	}

	// Get a new descriptor set if any parts have changed
	if (m_dirty_flags & DIRTY_FLAG_DESCRIPTOR_SET)
	{
		ERROR_LOG(VIDEO, "Failed to get descriptor set, skipping draw");
		return false;
	}

	// Re-bind parts of the pipeline

	if (m_dirty_flags & DIRTY_FLAG_VERTEX_BUFFER || rebind_all)
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);

	if (m_dirty_flags & DIRTY_FLAG_INDEX_BUFFER || rebind_all)
		vkCmdBindIndexBuffer(command_buffer, m_index_buffer, m_index_buffer_offset, m_index_type);

	if (m_dirty_flags & DIRTY_FLAG_PIPELINE || rebind_all)
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_object);

	if (m_dirty_flags & DIRTY_FLAG_DESCRIPTOR_SET || rebind_all)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_object_cache->GetPipelineLayout(), 0, 1, &m_descriptor_set, 0, nullptr);

	if (m_dirty_flags & DIRTY_FLAG_VIEWPORT || rebind_all)
		vkCmdSetViewport(command_buffer, 0, 1, &m_viewport);

	if (m_dirty_flags & DIRTY_FLAG_SCISSOR || rebind_all)
		vkCmdSetScissor(command_buffer, 0, 1, &m_scissor);

	m_dirty_flags = 0;
	return true;
}

bool StateTracker::UpdatePipeline()
{
	// We need at least a vertex and fragment shader
	if (m_pipeline_state.vs == VK_NULL_HANDLE || m_pipeline_state.ps == VK_NULL_HANDLE)
		return false;

	// Grab a new pipeline object, this can fail
	m_pipeline_object = m_object_cache->GetPipeline(m_pipeline_state);
	if (m_pipeline_object == VK_NULL_HANDLE)
		return false;

	return true;
}

bool StateTracker::UpdateDescriptorSet()
{
	std::array<VkWriteDescriptorSet, NUM_COMBINED_DESCRIPTOR_SET_BINDINGS + NUM_PIXEL_SHADER_SAMPLERS - 1> descriptor_set_writes;
	std::array<VkCopyDescriptorSet, NUM_COMBINED_DESCRIPTOR_SET_BINDINGS> descriptor_set_copies;
	uint32_t num_descriptor_set_writes = 0;
	uint32_t num_descriptor_set_copies = 0;

	// If there is an existing descriptor set, we copy as much as we can from it.
	VkDescriptorSet existing_descriptor_set = m_descriptor_set;

	// Allocate a new descriptor set
	VkDescriptorSet new_descriptor_set = m_command_buffer_mgr->AllocateDescriptorSet(m_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_COMBINED));
	if (new_descriptor_set == VK_NULL_HANDLE)
		return false;

	// VS uniform buffer
	if (m_bindings.vs_ubo.buffer != VK_NULL_HANDLE)
	{
		if (m_dirty_flags & DIRTY_FLAG_VS_UBO || existing_descriptor_set == VK_NULL_HANDLE)
		{
			// Write new buffer
			descriptor_set_writes[num_descriptor_set_writes++] = {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, new_descriptor_set,
				COMBINED_DESCRIPTOR_SET_BINDING_VS_UBO,
				0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				nullptr, &m_bindings.vs_ubo, nullptr
			};
		}
		else
		{
			// Use existing buffer
			descriptor_set_copies[num_descriptor_set_copies++] = {
				VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr,
				existing_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_VS_UBO, 0,
				new_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_VS_UBO, 0,
				1
			};
		}
	}

	// GS uniform buffer
	if (m_bindings.gs_ubo.buffer != VK_NULL_HANDLE)
	{
		if (m_dirty_flags & DIRTY_FLAG_GS_UBO || existing_descriptor_set == VK_NULL_HANDLE)
		{
			// Write new buffer
			descriptor_set_writes[num_descriptor_set_writes++] = {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, new_descriptor_set,
				COMBINED_DESCRIPTOR_SET_BINDING_GS_UBO,
				0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				nullptr, &m_bindings.gs_ubo, nullptr
			};
		}
		else
		{
			// Use existing buffer
			descriptor_set_copies[num_descriptor_set_copies++] = {
				VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr,
				existing_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_GS_UBO, 0,
				new_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_GS_UBO, 0,
				1
			};
		}
	}

	// PS uniform buffer
	if (m_bindings.ps_ubo.buffer != VK_NULL_HANDLE)
	{
		if (m_dirty_flags & DIRTY_FLAG_PS_UBO || existing_descriptor_set == VK_NULL_HANDLE)
		{
			// Write new buffer
			descriptor_set_writes[num_descriptor_set_writes++] = {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, new_descriptor_set,
				COMBINED_DESCRIPTOR_SET_BINDING_PS_UBO,
				0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				nullptr, &m_bindings.ps_ubo, nullptr
			};
		}
		else
		{
			// Use existing buffer
			descriptor_set_copies[num_descriptor_set_copies++] = {
				VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr,
				existing_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_PS_UBO, 0,
				new_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_PS_UBO, 0,
				1
			};
		}
	}

	// PS samplers
	// Check if we have any at all, skip the binding process entirely if we don't
	size_t first_active_ps_sampler;
	for (first_active_ps_sampler = 0; first_active_ps_sampler < NUM_PIXEL_SHADER_SAMPLERS; first_active_ps_sampler++)
	{
		if (m_bindings.ps_samplers[first_active_ps_sampler].imageView != VK_NULL_HANDLE)
			break;
	}
	if (first_active_ps_sampler != NUM_PIXEL_SHADER_SAMPLERS)
	{
		if (m_dirty_flags & DIRTY_FLAG_PS_SAMPLERS || existing_descriptor_set == VK_NULL_HANDLE)
		{
			// Initialize descriptor count to zero for the first image group.
			// The remaining fields will be initialized in the "create new group" branch below.
			descriptor_set_writes[num_descriptor_set_writes].descriptorCount = 0;

			// See Vulkan spec regarding descriptor copies, we can pack multiple bindings together in one update.
			for (size_t i = 0; i < NUM_PIXEL_SHADER_SAMPLERS; i++)
			{
				// Still batching together textures without any gaps?
				const VkDescriptorImageInfo& info = m_bindings.ps_samplers[i];
				if (info.imageView != VK_NULL_HANDLE && info.sampler != VK_NULL_HANDLE)
				{
					// Allocated a group yet?
					if (descriptor_set_writes[num_descriptor_set_writes].descriptorCount > 0)
					{
						// Add to the group.
						descriptor_set_writes[num_descriptor_set_writes].descriptorCount++;
					}
					else
					{
						// Create a new group.
						descriptor_set_writes[num_descriptor_set_writes] = {
							VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, new_descriptor_set,
							COMBINED_DESCRIPTOR_SET_BINDING_PS_SAMPLERS, static_cast<uint32_t>(i),
							1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							&info, nullptr, nullptr
						};
					}
				}
				else
				{
					// We've found an unbound sampler. If there is a non-zero number of images in the group,
					// and remaining images will have to be split into a separate group.
					if (descriptor_set_writes[num_descriptor_set_writes].descriptorCount > 0)
					{
						num_descriptor_set_writes++;
						descriptor_set_writes[num_descriptor_set_writes].descriptorCount = 0;
					}
				}
			}

			// Complete the image group if one has been started.
			if (descriptor_set_writes[num_descriptor_set_writes].descriptorCount > 0)
				num_descriptor_set_writes++;
		}
		else
		{
			// Use existing sampler set
			descriptor_set_copies[num_descriptor_set_copies++] = {
				VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr,
				existing_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_PS_SAMPLERS, 0,
				new_descriptor_set, COMBINED_DESCRIPTOR_SET_BINDING_PS_SAMPLERS, 0,
				static_cast<uint32_t>(NUM_PIXEL_SHADER_SAMPLERS)
			};
		}
	}

	// Upload descriptors
	vkUpdateDescriptorSets(m_object_cache->GetDevice(),
		num_descriptor_set_writes,
		(num_descriptor_set_writes > 0) ? descriptor_set_writes.data() : nullptr,
		num_descriptor_set_copies,
		(num_descriptor_set_copies > 0) ? descriptor_set_copies.data() : nullptr);

	// Swap out references
	m_descriptor_set = new_descriptor_set;
	return true;
}

} // namespace Vulkan
