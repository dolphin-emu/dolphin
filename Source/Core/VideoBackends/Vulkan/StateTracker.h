// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderBase.h"

namespace Vulkan
{
class VKPipeline;
class StreamBuffer;
class VertexFormat;

class StateTracker
{
public:
  StateTracker() = default;
  ~StateTracker() = default;

  static StateTracker* GetInstance();
  static bool CreateInstance();
  static void DestroyInstance();

  const std::array<VkDescriptorImageInfo, NUM_PIXEL_SHADER_SAMPLERS>& GetPSSamplerBindings() const
  {
    return m_bindings.ps_samplers;
  }
  VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
  const VKPipeline* GetPipeline() const { return m_pipeline; }
  void SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset);
  void SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type);

  void SetRenderPass(VkRenderPass load_render_pass, VkRenderPass clear_render_pass);
  void SetFramebuffer(VkFramebuffer framebuffer, const VkRect2D& render_area);
  void SetPipeline(const VKPipeline* pipeline);

  void UpdateVertexShaderConstants();
  void UpdateGeometryShaderConstants();
  void UpdatePixelShaderConstants();

  void SetTexture(size_t index, VkImageView view);
  void SetSampler(size_t index, VkSampler sampler);

  void SetBBoxBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

  void UnbindTexture(VkImageView view);

  // When executing a command buffer, we want to recreate the descriptor set, as it will
  // now be in a different pool for the new command buffer.
  void InvalidateDescriptorSets();

  // Same with the uniforms, as the current storage will belong to the previous command buffer.
  void InvalidateConstants();

  // Set dirty flags on everything to force re-bind at next draw time.
  void SetPendingRebind();

  // Ends a render pass if we're currently in one.
  // When Bind() is next called, the pass will be restarted.
  // Calling this function is allowed even if a pass has not begun.
  bool InRenderPass() const { return m_current_render_pass != VK_NULL_HANDLE; }
  void BeginRenderPass();
  void EndRenderPass();

  // Ends the current render pass if it was a clear render pass.
  void BeginClearRenderPass(const VkRect2D& area, const VkClearValue* clear_values,
                            u32 num_clear_values);
  void EndClearRenderPass();

  void SetViewport(const VkViewport& viewport);
  void SetScissor(const VkRect2D& scissor);

  bool Bind(bool rebind_all = false);

  // CPU Access Tracking
  // Call after a draw call is made.
  void OnDraw();

  // Call after CPU access is requested.
  void OnCPUEFBAccess();

  // Call after an EFB copy to RAM. If true, the current command buffer should be executed.
  void OnEFBCopyToRAM();

  // Call at the end of a frame.
  void OnEndFrame();

  // Prevent/allow background command buffer execution.
  // Use when queries are active.
  void SetBackgroundCommandBufferExecution(bool enabled);

  bool IsWithinRenderArea(s32 x, s32 y, u32 width, u32 height) const;

private:
  // Number of descriptor sets for game draws.
  enum
  {
    NUM_GX_DRAW_DESCRIPTOR_SETS = DESCRIPTOR_SET_BIND_POINT_STORAGE_OR_TEXEL_BUFFER + 1
  };

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

    DIRTY_FLAG_ALL_DESCRIPTOR_SETS = DIRTY_FLAG_VS_UBO | DIRTY_FLAG_GS_UBO | DIRTY_FLAG_PS_UBO |
                                     DIRTY_FLAG_PS_SAMPLERS | DIRTY_FLAG_PS_SSBO
  };

  bool Initialize();

  // Check that the specified viewport is within the render area.
  // If not, ends the render pass if it is a clear render pass.
  bool IsViewportWithinRenderArea() const;

  bool UpdateDescriptorSet();

  // Allocates storage in the uniform buffer of the specified size. If this storage cannot be
  // allocated immediately, the current command buffer will be submitted and all stage's
  // constants will be re-uploaded. false will be returned in this case, otherwise true.
  bool ReserveConstantStorage();
  void UploadAllConstants();

  // Which bindings/state has to be updated before the next draw.
  u32 m_dirty_flags = 0;

  // input assembly
  VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
  VkDeviceSize m_vertex_buffer_offset = 0;
  VkBuffer m_index_buffer = VK_NULL_HANDLE;
  VkDeviceSize m_index_buffer_offset = 0;
  VkIndexType m_index_type = VK_INDEX_TYPE_UINT16;

  // pipeline state
  const VKPipeline* m_pipeline = nullptr;

  // shader bindings
  std::array<VkDescriptorSet, NUM_DESCRIPTOR_SET_BIND_POINTS> m_descriptor_sets = {};
  struct
  {
    std::array<VkDescriptorBufferInfo, NUM_UBO_DESCRIPTOR_SET_BINDINGS> uniform_buffer_bindings =
        {};
    std::array<uint32_t, NUM_UBO_DESCRIPTOR_SET_BINDINGS> uniform_buffer_offsets = {};

    std::array<VkDescriptorImageInfo, NUM_PIXEL_SHADER_SAMPLERS> ps_samplers = {};

    VkDescriptorBufferInfo ps_ssbo = {};
  } m_bindings;
  size_t m_uniform_buffer_reserve_size = 0;
  u32 m_num_active_descriptor_sets = 0;

  // rasterization
  VkViewport m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  VkRect2D m_scissor = {{0, 0}, {1, 1}};

  // uniform buffers
  std::unique_ptr<StreamBuffer> m_uniform_stream_buffer;

  VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
  VkRenderPass m_load_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_clear_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_current_render_pass = VK_NULL_HANDLE;
  VkRect2D m_framebuffer_size = {};
  VkRect2D m_framebuffer_render_area = {};

  // CPU access tracking
  u32 m_draw_counter = 0;
  u32 m_last_efb_copy_draw_counter = 0;
  std::vector<u32> m_cpu_accesses_this_frame;
  std::vector<u32> m_scheduled_command_buffer_kicks;
  bool m_allow_background_execution = true;
};
}
