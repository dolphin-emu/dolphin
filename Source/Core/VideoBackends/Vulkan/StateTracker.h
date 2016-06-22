// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class ObjectCache;

class StateTracker
{
public:
	StateTracker(ObjectCache* object_cache);
	~StateTracker();

	bool CheckForShaderChanges(u32 primitive_type, DSTALPHA_MODE dstalpha_mode);
	
	void SetVSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
	void SetGSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
	void SetPSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

	void SetPSTexture(size_t index, VkImageView view);
	void SetPSSampler(size_t index, VkSampler sampler);

	//void SetPSBBoxBuffer()

	void SetViewport(const VkViewport& viewport);
	void SetScissor(const VkRect2D& scissor);

	bool Bind(VkCommandBuffer command_buffer, bool rebind_all = false);

private:
	ObjectCache* m_object_cache = nullptr;

	// shader state
	struct
	{
		VertexShaderUid vs_uid = {};
		GeometryShaderUid gs_uid = {};
		PixelShaderUid ps_uid = {};
		VkShaderModule vs_module = VK_NULL_HANDLE;
		VkShaderModule gs_module = VK_NULL_HANDLE;
		VkShaderModule ps_module = VK_NULL_HANDLE;
		u32 primitive_type = 0;
		DSTALPHA_MODE dstalpha_mode = DSTALPHA_NONE;
		RasterizationState rasterization_state = {};
		DepthStencilState depth_stencil_state = {};
		BlendState blend_state = {};
	} m_state;

	VkPipeline m_pipeline = VK_NULL_HANDLE;
	bool m_pipeline_changed = true;

	enum DITRY_BINDING_FLAG : u32
	{
		DIRTY_BINDING_FLAG_VS_UBO = (1 << 0),
		DIRTY_BINDING_FLAG_GS_UBO = (1 << 1),
		DIRTY_BINDING_FLAG_PS_UBO = (1 << 2),
		DIRTY_BINDING_FLAG_PS_SAMPLERS = (1 << 3),
		DIRTY_BINDING_FLAG_PS_SSBO = (1 << 4)
	};

	// shader bindings
	struct
	{
		VkDescriptorBufferInfo vs_ubo;
		VkDescriptorBufferInfo gs_ubo;
		VkDescriptorBufferInfo ps_ubo;
		VkDescriptorImageInfo ps_samplers[NUM_PIXEL_SHADER_SAMPLERS];
		VkDescriptorBufferInfo ps_ssbo;
	} m_bindings;
	u32 m_dirty_binding_flags = 0;

	// other stuff
	VkViewport m_viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	bool m_dirty_viewport = false;
	VkRect2D m_scissor = { { 0, 0 }, { 1, 1 } };
	bool m_dirty_scissor = false;
};

}
