// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/VulkanImports.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Vulkan
{
class StreamBuffer;
class VertexFormat;

class StateTracker
{
public:
  StateTracker();
  ~StateTracker();

  const RasterizationState& GetRasterizationState() const
  {
    return m_pipeline_state.rasterization_state;
  }
  const DepthStencilState& GetDepthStencilState() const
  {
    return m_pipeline_state.depth_stencil_state;
  }
  const BlendState& GetBlendState() const { return m_pipeline_state.blend_state; }
  void SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset);
  void SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type);

  void SetRenderPass(VkRenderPass render_pass);
  void SetFramebuffer(VkFramebuffer framebuffer, const VkRect2D& render_area);

  void SetVertexFormat(const VertexFormat* vertex_format);

  void SetPrimitiveTopology(VkPrimitiveTopology primitive_topology);

  void DisableBackFaceCulling();

  void SetRasterizationState(const RasterizationState& state);
  void SetDepthStencilState(const DepthStencilState& state);
  void SetBlendState(const BlendState& state);

  bool CheckForShaderChanges(u32 gx_primitive_type, DSTALPHA_MODE dstalpha_mode);

  void UpdateVertexShaderConstants();
  void UpdateGeometryShaderConstants();
  void UpdatePixelShaderConstants();

  void SetTexture(size_t index, VkImageView view);
  void SetSampler(size_t index, VkSampler sampler);

  void SetBBoxEnable(bool enable);
  void SetBBoxBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

  void UnbindTexture(VkImageView view);

  // When executing a command buffer, we want to recreate the descriptor set, as it will
  // now be in a different pool for the new command buffer.
  void InvalidateDescriptorSets();

  // Set dirty flags on everything to force re-bind at next draw time.
  void SetPendingRebind();

  // Ends a render pass if we're currently in one.
  // When Bind() is next called, the pass will be restarted.
  // Calling this function is allowed even if a pass has not begun.
  void BeginRenderPass();
  void EndRenderPass();

  void SetViewport(const VkViewport& viewport);
  void SetScissor(const VkRect2D& scissor);

  bool Bind(bool rebind_all = false);

private:
  bool UpdatePipeline();
  bool UpdateDescriptorSet();
  void UploadAllConstants();

  enum DITRY_FLAG : u32
  {
    DIRTY_FLAG_VS_UBO = (1 << 0),
    DIRTY_FLAG_GS_UBO = (1 << 1),
    DIRTY_FLAG_PS_UBO = (1 << 2),
    DIRTY_FLAG_PS_SAMPLERS = (1 << 3),
    DIRTY_FLAG_PS_SSBO = (1 << 4),
    DIRTY_FLAG_DYNAMIC_OFFSETS = (1 << 5),
    DIRTY_FLAG_VERTEX_BUFFER = (1 << 6),
    DIRTY_FLAG_INDEX_BUFFER = (1 << 7),
    DIRTY_FLAG_VIEWPORT = (1 << 8),
    DIRTY_FLAG_SCISSOR = (1 << 9),
    DIRTY_FLAG_PIPELINE = (1 << 10),
    DIRTY_FLAG_DESCRIPTOR_SET_BINDING = (1 << 11),
    DIRTY_FLAG_PIPELINE_BINDING = (1 << 12),

    DIRTY_FLAG_ALL_DESCRIPTOR_SETS =
        DIRTY_FLAG_VS_UBO | DIRTY_FLAG_GS_UBO | DIRTY_FLAG_PS_SAMPLERS | DIRTY_FLAG_PS_SSBO
  };
  u32 m_dirty_flags = 0;

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
  std::array<VkDescriptorSet, NUM_DESCRIPTOR_SETS> m_descriptor_sets = {};
  struct
  {
    std::array<VkDescriptorBufferInfo, NUM_UBO_DESCRIPTOR_SET_BINDINGS> uniform_buffer_bindings =
        {};
    std::array<uint32_t, NUM_UBO_DESCRIPTOR_SET_BINDINGS> uniform_buffer_offsets = {};

    std::array<VkDescriptorImageInfo, NUM_PIXEL_SHADER_SAMPLERS> ps_samplers = {};

    VkDescriptorBufferInfo ps_ssbo = {};
  } m_bindings;
  u32 m_num_active_descriptor_sets = 0;

  // rasterization
  VkViewport m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  VkRect2D m_scissor = {{0, 0}, {1, 1}};

  // uniform buffers
  std::unique_ptr<StreamBuffer> m_uniform_stream_buffer;

  VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
  VkRect2D m_framebuffer_render_area = {};
  bool m_in_render_pass = false;
  bool m_bbox_enabled = false;
};

}
