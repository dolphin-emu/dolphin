// Copyright 2016 Dolphin Emulator Project
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
class VertexFormat;

class StateTracker
{
public:
	StateTracker(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr);
	~StateTracker();

	void SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset);
	void SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type);

	void SetRenderPass(VkRenderPass render_pass);

	void SetVertexFormat(const VertexFormat* vertex_format);

	void SetPrimitiveTopology(VkPrimitiveTopology primitive_topology);

	void DisableBackFaceCulling();

	void SetRasterizationState(const RasterizationState& state);
	void SetDepthStencilState(const DepthStencilState& state);
	void SetBlendState(const BlendState& state);

	u32 GetColorWriteMask() const { return m_pipeline_state.blend_state.write_mask; }
	void SetColorMask(u32 mask);

	bool CheckForShaderChanges(u32 gx_primitive_type, DSTALPHA_MODE dstalpha_mode);

	void SetVSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
	void SetGSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
	void SetPSUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

	void SetPSTexture(size_t index, VkImageView view);
	void SetPSSampler(size_t index, VkSampler sampler);

	//void SetPSBBoxBuffer()

	// When executing a command buffer, we want to recreate the descriptor set, as it will
	// now be in a different pool for the new command buffer.
	void InvalidateDescriptorSet();

	void SetViewport(const VkViewport& viewport);
	void SetScissor(const VkRect2D& scissor);

	bool Bind(VkCommandBuffer command_buffer, bool rebind_all = false);

private:
	bool UpdatePipeline();
	bool UpdateDescriptorSet();

	ObjectCache* m_object_cache = nullptr;
	CommandBufferManager* m_command_buffer_mgr = nullptr;

	enum DITRY_FLAG : u32
	{
		DIRTY_FLAG_VS_UBO = (1 << 0),
		DIRTY_FLAG_GS_UBO = (1 << 1),
		DIRTY_FLAG_PS_UBO = (1 << 2),
		DIRTY_FLAG_PS_SAMPLERS = (1 << 3),
		DIRTY_FLAG_PS_SSBO = (1 << 4),
		DIRTY_FLAG_VERTEX_BUFFER = (1 << 5),
		DIRTY_FLAG_INDEX_BUFFER = (1 << 6),
		DIRTY_FLAG_VIEWPORT = (1 << 7),
		DIRTY_FLAG_SCISSOR = (1 << 8),
		DIRTY_FLAG_PIPELINE = (1 << 9),
		DIRTY_FLAG_DESCRIPTOR_SET = (1 << 10)
	};
	u32 m_dirty_flags = 0xFFFFFFFF;

	// input assembly
	VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
	VkDeviceSize m_vertex_buffer_offset = 0;
	VkBuffer m_index_buffer = VK_NULL_HANDLE;
	VkDeviceSize m_index_buffer_offset = 0;
	VkIndexType m_index_type = VK_INDEX_TYPE_UINT16;

	// shader state
	VertexShaderUid m_vs_uid = {};
	GeometryShaderUid m_gs_uid = {};
	PixelShaderUid m_ps_uid = {};

	// pipeline state
	PipelineInfo m_pipeline_state = {};
	u32 m_gx_primitive_type = 0;
	DSTALPHA_MODE m_dstalpha_mode = DSTALPHA_NONE;
	VkPipeline m_pipeline_object = VK_NULL_HANDLE;

	// shader bindings
	VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
	struct
	{
		VkDescriptorBufferInfo vs_ubo;
		VkDescriptorBufferInfo gs_ubo;
		VkDescriptorBufferInfo ps_ubo;
		VkDescriptorImageInfo ps_samplers[NUM_PIXEL_SHADER_SAMPLERS];
		VkDescriptorBufferInfo ps_ssbo;
	} m_bindings;

	// other stuff
	VkViewport m_viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	VkRect2D m_scissor = { { 0, 0 }, { 1, 1 } };
};

}
