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

bool StateTracker::CheckForShaderChanges(u32 primitive_type, DSTALPHA_MODE dstalpha_mode)
{
	VertexShaderUid vs_uid = GetVertexShaderUid(API_OPENGL);
	GeometryShaderUid gs_uid = GetGeometryShaderUid(m_state.primitive_type, API_OPENGL);
	PixelShaderUid ps_uid = GetPixelShaderUid(dstalpha_mode, API_OPENGL);

	bool changed = false;

	if (vs_uid != m_state.vs_uid)
	{
		m_state.vs_module = m_object_cache->GetVertexShaderCache().GetShaderForUid(vs_uid, primitive_type, dstalpha_mode);
		m_state.vs_uid = vs_uid;
		changed = true;
	}

	if (gs_uid != m_state.gs_uid)
	{
		m_state.gs_module = m_object_cache->GetGeometryShaderCache().GetShaderForUid(gs_uid, primitive_type, dstalpha_mode);
		m_state.gs_uid = gs_uid;
		changed = true;
	}

	if (ps_uid != m_state.ps_uid)
	{
		m_state.ps_module = m_object_cache->GetPixelShaderCache().GetShaderForUid(ps_uid, primitive_type, dstalpha_mode);
		m_state.ps_uid = ps_uid;
		changed = true;
	}

	if (m_state.primitive_type != primitive_type)
	{
		m_state.primitive_type = primitive_type;
		changed = true;
	}

	if (m_state.dstalpha_mode != dstalpha_mode)
	{
		m_state.dstalpha_mode = dstalpha_mode;
		changed = true;
	}

	if (changed)
		m_pipeline_changed = true;

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
	m_dirty_binding_flags |= DIRTY_BINDING_FLAG_VS_UBO;
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
	m_dirty_binding_flags |= DIRTY_BINDING_FLAG_GS_UBO;
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
	m_dirty_binding_flags |= DIRTY_BINDING_FLAG_PS_UBO;
}

void StateTracker::SetPSTexture(size_t index, VkImageView view)
{
	if (m_bindings.ps_samplers[index].imageView == view)
		return;

	m_bindings.ps_samplers[index].imageView = view;
	m_bindings.ps_samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_dirty_binding_flags |= DIRTY_BINDING_FLAG_PS_SAMPLERS;
}

void StateTracker::SetPSSampler(size_t index, VkSampler sampler)
{
	if (m_bindings.ps_samplers[index].sampler == sampler)
		return;

	m_bindings.ps_samplers[index].sampler = sampler;
	m_dirty_binding_flags |= DIRTY_BINDING_FLAG_PS_SAMPLERS;
}

void StateTracker::SetViewport(const VkViewport& viewport)
{
	if (memcmp(&m_viewport, &viewport, sizeof(viewport)) == 0)
		return;

	m_viewport = viewport;
	m_dirty_viewport = true;
}

void StateTracker::SetScissor(const VkRect2D& scissor)
{
	if (memcmp(&m_scissor, &scissor, sizeof(scissor)) == 0)
		return;

	m_scissor = scissor;
	m_dirty_scissor = true;
}

bool StateTracker::Bind(VkCommandBuffer command_buffer, bool rebind_all /*= false*/)
{
	if (m_pipeline_changed)
	{
		// Grab a new pipeline object, this can fail
		//m_object_cache->GetPipeline()
		m_pipeline = VK_NULL_HANDLE;
	}

	// Can't draw without a pipeline, leave the rest
	if (m_pipeline == VK_NULL_HANDLE)
		return false;

	// TODO: shader bindings
	// only update the parts of the descriptor set that are new, copy the old

	if (m_dirty_viewport || rebind_all)
	{
		vkCmdSetViewport(command_buffer, 0, 1, &m_viewport);
		m_dirty_viewport = false;
	}

	if (m_dirty_scissor || rebind_all)
	{
		vkCmdSetScissor(command_buffer, 0, 1, &m_scissor);
		m_dirty_scissor = false;
	}

	return true;
}

} // namespace Vulkan
