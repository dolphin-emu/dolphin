// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"

namespace Vulkan
{
class CommandBufferManager;
class StateTracker;

namespace Util
{
size_t AlignBufferOffset(size_t offset, size_t alignment);

u32 MakeRGBA8Color(float r, float g, float b, float a);

bool IsDepthFormat(VkFormat format);
bool IsCompressedFormat(VkFormat format);
VkFormat GetLinearFormat(VkFormat format);
VkFormat GetVkFormatForHostTextureFormat(AbstractTextureFormat format);
u32 GetTexelSize(VkFormat format);
u32 GetBlockSize(VkFormat format);

// Clamps a VkRect2D to the specified dimensions.
VkRect2D ClampRect2D(const VkRect2D& rect, u32 width, u32 height);

// Map {SRC,DST}_COLOR to {SRC,DST}_ALPHA
VkBlendFactor GetAlphaBlendFactor(VkBlendFactor factor);

RasterizationState GetNoCullRasterizationState();
DepthStencilState GetNoDepthTestingDepthStencilState();
BlendingState GetNoBlendingBlendState();

// Combines viewport and scissor updates
void SetViewportAndScissor(VkCommandBuffer command_buffer, int x, int y, int width, int height,
                           float min_depth = 0.0f, float max_depth = 1.0f);

// Wrapper for creating an barrier on a buffer
void BufferMemoryBarrier(VkCommandBuffer command_buffer, VkBuffer buffer,
                         VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
                         VkDeviceSize offset, VkDeviceSize size,
                         VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask);

// Completes the current render pass, executes the command buffer, and restores state ready for next
// render. Use when you want to kick the current buffer to make room for new data.
void ExecuteCurrentCommandsAndRestoreState(bool execute_off_thread,
                                           bool wait_for_completion = false);

// Create a shader module from the specified SPIR-V.
VkShaderModule CreateShaderModule(const u32* spv, size_t spv_word_count);

// Compile a vertex shader and create a shader module, discarding the intermediate SPIR-V.
VkShaderModule CompileAndCreateVertexShader(const std::string& source_code);

// Compile a geometry shader and create a shader module, discarding the intermediate SPIR-V.
VkShaderModule CompileAndCreateGeometryShader(const std::string& source_code);

// Compile a fragment shader and create a shader module, discarding the intermediate SPIR-V.
VkShaderModule CompileAndCreateFragmentShader(const std::string& source_code);

// Compile a compute shader and create a shader module, discarding the intermediate SPIR-V.
VkShaderModule CompileAndCreateComputeShader(const std::string& source_code);
}

// Utility shader vertex format
#pragma pack(push, 1)
struct UtilityShaderVertex
{
  float Position[4];
  float TexCoord[4];
  u32 Color;

  void SetPosition(float x, float y)
  {
    Position[0] = x;
    Position[1] = y;
    Position[2] = 0.0f;
    Position[3] = 1.0f;
  }
  void SetPosition(float x, float y, float z)
  {
    Position[0] = x;
    Position[1] = y;
    Position[2] = z;
    Position[3] = 1.0f;
  }
  void SetTextureCoordinates(float u, float v)
  {
    TexCoord[0] = u;
    TexCoord[1] = v;
    TexCoord[2] = 0.0f;
    TexCoord[3] = 0.0f;
  }
  void SetTextureCoordinates(float u, float v, float w)
  {
    TexCoord[0] = u;
    TexCoord[1] = v;
    TexCoord[2] = w;
    TexCoord[3] = 0.0f;
  }
  void SetTextureCoordinates(float u, float v, float w, float x)
  {
    TexCoord[0] = u;
    TexCoord[1] = v;
    TexCoord[2] = w;
    TexCoord[3] = x;
  }
  void SetColor(u32 color) { Color = color; }
  void SetColor(float r, float g, float b) { Color = Util::MakeRGBA8Color(r, g, b, 1.0f); }
  void SetColor(float r, float g, float b, float a) { Color = Util::MakeRGBA8Color(r, g, b, a); }
};
#pragma pack(pop)

class UtilityShaderDraw
{
public:
  UtilityShaderDraw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout,
                    VkRenderPass render_pass, VkShaderModule vertex_shader,
                    VkShaderModule geometry_shader, VkShaderModule pixel_shader);

  UtilityShaderVertex* ReserveVertices(VkPrimitiveTopology topology, size_t count);
  void CommitVertices(size_t count);

  void UploadVertices(VkPrimitiveTopology topology, UtilityShaderVertex* vertices, size_t count);

  u8* AllocateVSUniforms(size_t size);
  void CommitVSUniforms(size_t size);

  u8* AllocatePSUniforms(size_t size);
  void CommitPSUniforms(size_t size);

  void SetPushConstants(const void* data, size_t data_size);

  void SetPSSampler(size_t index, VkImageView view, VkSampler sampler);

  void SetPSTexelBuffer(VkBufferView view);

  void SetRasterizationState(const RasterizationState& state);
  void SetDepthStencilState(const DepthStencilState& state);
  void SetBlendState(const BlendingState& state);

  void BeginRenderPass(VkFramebuffer framebuffer, const VkRect2D& region,
                       const VkClearValue* clear_value = nullptr);
  void EndRenderPass();

  void Draw();

  // NOTE: These methods alter the viewport state of the command buffer.

  // Sets texture coordinates to 0..1
  void DrawQuad(int x, int y, int width, int height, float z = 0.0f);

  // Sets texture coordinates to the specified range
  void DrawQuad(int dst_x, int dst_y, int dst_width, int dst_height, int src_x, int src_y,
                int src_layer, int src_width, int src_height, int src_full_width,
                int src_full_height, float z = 0.0f);

  void DrawColoredQuad(int x, int y, int width, int height, u32 color, float z = 0.0f);

  void DrawColoredQuad(int x, int y, int width, int height, float r, float g, float b, float a,
                       float z = 0.0f);

  // Draw without a vertex buffer. Assumes viewport has been initialized separately.
  void SetViewportAndScissor(int x, int y, int width, int height);
  void DrawWithoutVertexBuffer(VkPrimitiveTopology primitive_topology, u32 vertex_count);

private:
  void BindVertexBuffer();
  void BindDescriptors();
  bool BindPipeline();

  VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
  VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
  VkDeviceSize m_vertex_buffer_offset = 0;
  uint32_t m_vertex_count = 0;

  VkDescriptorBufferInfo m_vs_uniform_buffer = {};
  VkDescriptorBufferInfo m_ps_uniform_buffer = {};
  std::array<uint32_t, NUM_UBO_DESCRIPTOR_SET_BINDINGS> m_ubo_offsets = {};

  std::array<VkDescriptorImageInfo, NUM_PIXEL_SHADER_SAMPLERS> m_ps_samplers = {};

  VkBufferView m_ps_texel_buffer = VK_NULL_HANDLE;

  PipelineInfo m_pipeline_info = {};
};

class ComputeShaderDispatcher
{
public:
  ComputeShaderDispatcher(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout,
                          VkShaderModule compute_shader);

  u8* AllocateUniformBuffer(size_t size);
  void CommitUniformBuffer(size_t size);

  void SetPushConstants(const void* data, size_t data_size);

  void SetSampler(size_t index, VkImageView view, VkSampler sampler);

  void SetTexelBuffer(size_t index, VkBufferView view);

  void SetStorageImage(VkImageView view, VkImageLayout image_layout);

  void Dispatch(u32 groups_x, u32 groups_y, u32 groups_z);

private:
  void BindDescriptors();
  bool BindPipeline();

  VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;

  VkDescriptorBufferInfo m_uniform_buffer = {};
  u32 m_uniform_buffer_offset = 0;

  std::array<VkDescriptorImageInfo, 4> m_samplers = {};

  std::array<VkBufferView, 2> m_texel_buffers = {};

  VkDescriptorImageInfo m_storage_image = {};

  ComputePipelineInfo m_pipeline_info = {};
};

}  // namespace Vulkan
