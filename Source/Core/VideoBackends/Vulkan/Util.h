// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;
class StateTracker;

namespace Util {

size_t AlignValue(size_t value, size_t alignment);
size_t AlignBufferOffset(size_t offset, size_t alignment);

RasterizationState GetNoCullRasterizationState();
DepthStencilState GetNoDepthTestingDepthStencilState();
BlendState GetNoBlendingBlendState();

// Combines viewport and scissor updates
void SetViewportAndScissor(VkCommandBuffer command_buffer, int x, int y, int width, int height, float min_depth = 0.0f, float max_depth = 1.0f);

// Wrapper for creating an barrier on a buffer
void BufferMemoryBarrier(VkCommandBuffer command_buffer,
						 VkBuffer buffer,
						 VkAccessFlags src_access_mask,
						 VkAccessFlags dst_access_mask,
						 VkDeviceSize offset,
						 VkDeviceSize size,
						 VkPipelineStageFlags src_stage_mask,
						 VkPipelineStageFlags dst_stage_mask);

// Completes the current render pass, executes the command buffer, and restores state ready for next render.
// Use when you want to kick the current buffer to make room for new data.
void ExecuteCurrentCommandsAndRestoreState(CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker);

}

// Utility shader vertex format
#pragma pack(push, 1)
struct UtilityShaderVertex
{
	float Position[4];
	float TexCoord[4];
	float Color[4];

	void SetPosition(float x, float y) { Position[0] = x; Position[1] = y; Position[2] = 0.0f; Position[3] = 1.0f; }
	void SetPosition(float x, float y, float z) { Position[0] = x; Position[1] = y; Position[2] = z; Position[3] = 1.0f; }
	void SetTextureCoordinates(float u, float v) { TexCoord[0] = u; TexCoord[1] = v; TexCoord[2] = 0.0f; TexCoord[3] = 0.0f; }
	void SetTextureCoordinates(float u, float v, float w) { TexCoord[0] = u; TexCoord[1] = v; TexCoord[2] = w; TexCoord[3] = 0.0f; }
	void SetTextureCoordinates(float u, float v, float w, float x) { TexCoord[0] = u; TexCoord[1] = v; TexCoord[2] = w; TexCoord[3] = x; }
	void SetColor(float r, float g, float b) { Color[0] = r; Color[1] = g; Color[2] = b; Color[3] = 1.0f; }
	void SetColor(float r, float g, float b, float a) { Color[0] = r; Color[1] = g; Color[2] = b; Color[3] = a; }
};
#pragma pack(pop)

class UtilityShaderDraw
{
public:
	UtilityShaderDraw(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkRenderPass render_pass, VkShaderModule vertex_shader, VkShaderModule geometry_shader, VkShaderModule pixel_shader);

	UtilityShaderVertex* ReserveVertices(VkPrimitiveTopology topology, size_t count);
	void CommitVertices(size_t count);

	void UploadVertices(VkPrimitiveTopology topology, UtilityShaderVertex* vertices, size_t count);

	u8* AllocateVSUniforms(size_t size);
	void CommitVSUniforms(size_t size);

	u8* AllocatePSUniforms(size_t size);
	void CommitPSUniforms(size_t size);

	void SetPSSampler(size_t index, VkImageView view, VkSampler sampler);

	void SetRasterizationState(const RasterizationState& state);
	void SetDepthStencilState(const DepthStencilState& state);
	void SetBlendState(const BlendState& state);

    void BeginRenderPass(VkFramebuffer framebuffer, const VkRect2D& region, const VkClearValue* clear_value = nullptr);
	void EndRenderPass();

	void Draw();

	// NOTE: These methods alter the viewport state of the command buffer.

	// Sets texture coordinates to 0..1
	void DrawQuad(int x, int y, int width, int height);

	// Sets texture coordinates to the specified range
	void DrawQuad(int src_x, int src_y, int src_width, int src_height, int src_full_width, int src_full_height, int dst_x, int dst_y, int dst_width, int dst_height);

	void DrawColoredQuad(int x, int y, int width, int height, float r, float g, float b, float a);

private:
	void BindVertexBuffer(VkCommandBuffer command_buffer);
	void BindDescriptors(VkCommandBuffer command_buffer);
	bool BindPipeline(VkCommandBuffer command_buffer);

	ObjectCache* m_object_cache = nullptr;
	CommandBufferManager* m_command_buffer_mgr = nullptr;

	VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
	VkDeviceSize m_vertex_buffer_offset = 0;
	uint32_t m_vertex_count = 0;

	VkDescriptorBufferInfo m_vs_uniform_buffer = {};
	VkDescriptorBufferInfo m_ps_uniform_buffer = {};

	std::array<VkDescriptorImageInfo, NUM_PIXEL_SHADER_SAMPLERS> m_ps_samplers = {};

	PipelineInfo m_pipeline_info = {};
};

}		// namespace Vulkan
