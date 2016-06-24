// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>

#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"

#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/PixelShaderManager.h"

namespace Vulkan {

StateTracker::StateTracker(ObjectCache* object_cache)
	: m_object_cache(object_cache)
{

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

bool StateTracker::CheckForShaderChanges(u32 gx_primitive_type, DSTALPHA_MODE dstalpha_mode)
{
	VertexShaderUid vs_uid = GetVertexShaderUid(API_OPENGL);
	GeometryShaderUid gs_uid = GetGeometryShaderUid(gx_primitive_type, API_OPENGL);
	PixelShaderUid ps_uid = GetPixelShaderUid(dstalpha_mode, API_OPENGL);

	bool changed = false;

	if (vs_uid != m_vs_uid)
	{
		m_pipeline_state.vs = m_object_cache->GetVertexShaderCache().GetShaderForUid(vs_uid, gx_primitive_type, dstalpha_mode);
		m_vs_uid = vs_uid;
		changed = true;
	}

	if (gs_uid != m_gs_uid)
	{
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
	m_dirty_flags |= DIRTY_FLAG_VS_UBO;
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
	m_dirty_flags |= DIRTY_FLAG_GS_UBO;
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
	m_dirty_flags |= DIRTY_FLAG_PS_UBO;
}

void StateTracker::SetPSTexture(size_t index, VkImageView view)
{
	if (m_bindings.ps_samplers[index].imageView == view)
		return;

	m_bindings.ps_samplers[index].imageView = view;
	m_bindings.ps_samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_dirty_flags |= DIRTY_FLAG_PS_SAMPLERS;
}

void StateTracker::SetPSSampler(size_t index, VkSampler sampler)
{
	if (m_bindings.ps_samplers[index].sampler == sampler)
		return;

	m_bindings.ps_samplers[index].sampler = sampler;
	m_dirty_flags |= DIRTY_FLAG_PS_SAMPLERS;
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
	if (m_dirty_flags & DIRTY_FLAG_PIPELINE)
	{
		// We need at least a vertex and fragment shader
		if (m_pipeline_state.vs == VK_NULL_HANDLE || m_pipeline_state.ps == VK_NULL_HANDLE)
			return false;

		// Grab a new pipeline object, this can fail
		m_pipeline_object = m_object_cache->GetPipeline(m_pipeline_state);
		if (m_pipeline_object == VK_NULL_HANDLE)
		{
			ERROR_LOG(VIDEO, "Failed to get pipeline object, skipping draw");
			return false;
		}
	}

	// Input assembly
	if (m_dirty_flags & DIRTY_FLAG_VERTEX_BUFFER || rebind_all)
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);
	if (m_dirty_flags & DIRTY_FLAG_INDEX_BUFFER || rebind_all)
		vkCmdBindIndexBuffer(command_buffer, m_index_buffer, m_index_buffer_offset, m_index_type);

	// Pipeline
	if (m_dirty_flags & DIRTY_FLAG_PIPELINE || rebind_all)
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_object);

	// TODO: shader bindings
	// only update the parts of the descriptor set that are new, copy the old

	if (m_dirty_flags & DIRTY_FLAG_VIEWPORT || rebind_all)
		vkCmdSetViewport(command_buffer, 0, 1, &m_viewport);

	if (m_dirty_flags & DIRTY_FLAG_SCISSOR || rebind_all)
		vkCmdSetScissor(command_buffer, 0, 1, &m_scissor);

	m_dirty_flags = 0;
	return true;
}

} // namespace Vulkan
