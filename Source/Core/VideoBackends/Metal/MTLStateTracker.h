// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.h>
#include <atomic>
#include <memory>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#include "VideoBackends/Metal/MRCHelpers.h"
#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLTexture.h"
#include "VideoBackends/Metal/MTLUtil.h"

#include "VideoCommon/Constants.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/PerfQueryBase.h"

namespace Metal
{
class Pipeline;
class ComputePipeline;

class StateTracker
{
public:
  enum class UploadBuffer
  {
    Other,
    Uniform,
    Vertex,
    Index,
    Texels,
    Last = Texels
  };

  struct Map
  {
    id<MTLBuffer> gpu_buffer;
    size_t gpu_offset;
    void* cpu_buffer;
  };

  enum class AlignMask : size_t
  {
    None = 0,
    Other = 15,
    Uniform = 255,
  };

  StateTracker(StateTracker&&) = delete;
  explicit StateTracker();
  ~StateTracker();

  Framebuffer* GetCurrentFramebuffer() { return m_current_framebuffer; };
  void SetCurrentFramebuffer(Framebuffer* framebuffer);
  void BeginClearRenderPass(MTLClearColor color, float depth);
  void BeginRenderPass(MTLLoadAction load_action);
  void BeginRenderPass(MTLRenderPassDescriptor* descriptor);
  void BeginComputePass();
  MTLRenderPassDescriptor* GetRenderPassDescriptor(Framebuffer* framebuffer,
                                                   MTLLoadAction load_action);

  void EndRenderPass();
  void FlushEncoders();
  void WaitForFlushedEncoders();
  bool HasUnflushedData() { return static_cast<bool>(m_current_render_cmdbuf); }
  bool GPUBusy()
  {
    return m_current_draw != 1 + m_last_finished_draw.load(std::memory_order_acquire);
  }
  void ReloadSamplers();
  void NotifyOfCPUGPUSync()
  {
    if (!g_features.manual_buffer_upload || !m_manual_buffer_upload)
      return;
    if (m_upload_cmdbuf || m_current_render_cmdbuf)
      return;
    SetManualBufferUpload(false);
  }

  void SetPipeline(const Pipeline* pipe);
  void SetPipeline(const ComputePipeline* pipe);
  void SetScissor(const MathUtil::Rectangle<int>& rect);
  void SetViewport(float x, float y, float width, float height, float near_depth, float far_depth);
  void SetTexture(u32 idx, id<MTLTexture> texture);
  void SetSampler(u32 idx, const SamplerState& sampler);
  void InvalidateUniforms(bool vertex, bool geometry, bool fragment);
  void SetUtilityUniform(const void* buffer, size_t size);
  void SetTexelBuffer(id<MTLBuffer> buffer, u32 offset0, u32 offset1);
  void SetVerticesAndIndices(id<MTLBuffer> vertices, id<MTLBuffer> indices);
  void SetBBoxBuffer(id<MTLBuffer> bbox, id<MTLFence> upload, id<MTLFence> download);
  void SetVertexBufferNow(u32 idx, id<MTLBuffer> buffer, u32 offset);
  void SetFragmentBufferNow(u32 idx, id<MTLBuffer> buffer, u32 offset);
  /// Use around utility draws that are commonly used immediately before gx draws to the same buffer
  void EnableEncoderLabel(bool enabled) { m_flags.should_apply_label = enabled; }
  void EnablePerfQuery(PerfQueryGroup group, u32 query_id);
  void DisablePerfQuery();
  void UnbindTexture(id<MTLTexture> texture);

  void Draw(u32 base_vertex, u32 num_vertices);
  void DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex);
  void DispatchComputeShader(u32 groupsize_x, u32 groupsize_y, u32 groupsize_z, u32 groups_x,
                             u32 groups_y, u32 groups_z);
  void ResolveTexture(id<MTLTexture> src, id<MTLTexture> dst, u32 layer, u32 level);

  size_t Align(size_t amt, AlignMask align)
  {
    return (amt + static_cast<size_t>(align)) & ~static_cast<size_t>(align);
  }
  Map AllocateForTextureUpload(size_t amt);
  Map Allocate(UploadBuffer buffer_idx, size_t amt, AlignMask align)
  {
    Preallocate(buffer_idx, amt);
    return CommitPreallocation(buffer_idx, amt, align);
  }
  std::pair<void*, size_t> Preallocate(UploadBuffer buffer_idx, size_t amt);
  /// Must follow a call to Preallocate where amt is >= to the one provided here
  Map CommitPreallocation(UploadBuffer buffer_idx, size_t amt, AlignMask align)
  {
    DEBUG_ASSERT((m_upload_buffers[static_cast<int>(buffer_idx)].usage.Pos() &
                  static_cast<size_t>(align)) == 0);
    return CommitPreallocation(buffer_idx, Align(amt, align));
  }
  id<MTLBlitCommandEncoder> GetUploadEncoder();
  id<MTLBlitCommandEncoder> GetTextureUploadEncoder();
  id<MTLCommandBuffer> GetRenderCmdBuf();

private:
  class UsageTracker
  {
    struct UsageEntry
    {
      u64 drawno;
      size_t pos;
    };
    std::vector<UsageEntry> m_usage;
    size_t m_size = 0;
    size_t m_pos = 0;

  public:
    size_t Size() { return m_size; }
    size_t Pos() { return m_pos; }
    bool PrepareForAllocation(u64 last_draw, size_t amt);
    size_t Allocate(u64 current_draw, size_t amt);
    void Reset(size_t new_size);
  };

  struct CPUBuffer
  {
    UsageTracker usage;
    MRCOwned<id<MTLBuffer>> mtlbuffer;
    void* buffer = nullptr;
  };

  struct BufferPair
  {
    UsageTracker usage;
    MRCOwned<id<MTLBuffer>> cpubuffer;
    MRCOwned<id<MTLBuffer>> gpubuffer;
    void* buffer = nullptr;
    size_t last_upload = 0;
  };

  struct Backref;
  struct PerfQueryTracker;

  std::shared_ptr<Backref> m_backref;
  std::vector<std::shared_ptr<PerfQueryTracker>> m_perf_query_tracker_cache;
  MRCOwned<id<MTLFence>> m_fence;
  MRCOwned<id<MTLCommandBuffer>> m_upload_cmdbuf;
  MRCOwned<id<MTLBlitCommandEncoder>> m_upload_encoder;
  MRCOwned<id<MTLCommandBuffer>> m_texture_upload_cmdbuf;
  MRCOwned<id<MTLBlitCommandEncoder>> m_texture_upload_encoder;
  MRCOwned<id<MTLCommandBuffer>> m_current_render_cmdbuf;
  MRCOwned<id<MTLCommandBuffer>> m_last_render_cmdbuf;
  MRCOwned<id<MTLRenderCommandEncoder>> m_current_render_encoder;
  MRCOwned<id<MTLComputeCommandEncoder>> m_current_compute_encoder;
  MRCOwned<MTLRenderPassDescriptor*> m_resolve_pass_desc;
  Framebuffer* m_current_framebuffer;
  CPUBuffer m_texture_upload_buffer;
  BufferPair m_upload_buffers[static_cast<int>(UploadBuffer::Last) + 1];
  u64 m_current_draw = 1;
  std::atomic<u64> m_last_finished_draw{0};

  MRCOwned<id<MTLTexture>> m_dummy_texture;

  // Compute has a set of samplers and a set of writable images
  static constexpr u32 MAX_COMPUTE_TEXTURES = VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS * 2;
  static constexpr u32 MAX_PIXEL_TEXTURES = VideoCommon::MAX_PIXEL_SHADER_SAMPLERS;
  static constexpr u32 MAX_TEXTURES = std::max(MAX_PIXEL_TEXTURES, MAX_COMPUTE_TEXTURES);
  static constexpr u32 MAX_SAMPLERS =
      std::max(VideoCommon::MAX_PIXEL_SHADER_SAMPLERS, VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS);

  // MARK: State
  u16 m_dirty_textures;
  u16 m_dirty_samplers;
  static_assert(sizeof(m_dirty_textures) * 8 >= MAX_TEXTURES, "Make this bigger");
  static_assert(sizeof(m_dirty_samplers) * 8 >= MAX_SAMPLERS, "Make this bigger");
  union Flags
  {
    struct
    {
      // clang-format off
      bool has_gx_vs_uniform      : 1;
      bool has_gx_gs_uniform      : 1;
      bool has_gx_ps_uniform      : 1;
      bool has_utility_vs_uniform : 1;
      bool has_utility_ps_uniform : 1;
      bool has_pipeline           : 1;
      bool has_scissor            : 1;
      bool has_viewport           : 1;
      bool has_vertices           : 1;
      bool has_texel_buffer       : 1;
      bool bbox_fence             : 1;
      bool should_apply_label     : 1;
      // clang-format on
    };
    u16 bits = 0;
    void NewEncoder()
    {
      Flags reset_mask;
      // Set the flags you *don't* want to reset
      reset_mask.should_apply_label = true;
      bits &= reset_mask.bits;
    }
  } m_flags;

  /// Things that represent the state of the encoder
  struct Current
  {
    NSString* label;
    id<MTLRenderPipelineState> pipeline;
    std::array<id<MTLBuffer>, 2> vertex_buffers;
    std::array<id<MTLBuffer>, 3> fragment_buffers;
    u32 width;
    u32 height;
    MathUtil::Rectangle<int> scissor_rect;
    Util::Viewport viewport;
    MTLDepthClipMode depth_clip_mode;
    MTLCullMode cull_mode;
    DepthStencilSelector depth_stencil;
    PerfQueryGroup perf_query_group;
  } m_current;
  std::shared_ptr<PerfQueryTracker> m_current_perf_query;

  /// Things that represent what we'd *like* to have on the encoder for the next draw
  struct State
  {
    MathUtil::Rectangle<int> scissor_rect;
    Util::Viewport viewport;
    const Pipeline* render_pipeline = nullptr;
    const ComputePipeline* compute_pipeline = nullptr;
    std::array<id<MTLTexture>, MAX_TEXTURES> textures = {};
    std::array<id<MTLSamplerState>, MAX_SAMPLERS> samplers = {};
    std::array<float, MAX_SAMPLERS> sampler_min_lod;
    std::array<float, MAX_SAMPLERS> sampler_max_lod;
    std::array<SamplerState, MAX_SAMPLERS> sampler_states;
    const Texture* compute_texture = nullptr;
    std::unique_ptr<u8[]> utility_uniform;
    u32 utility_uniform_size = 0;
    u32 utility_uniform_capacity = 0;
    id<MTLBuffer> bbox = nullptr;
    id<MTLFence> bbox_upload_fence = nullptr;
    id<MTLFence> bbox_download_fence = nullptr;
    id<MTLBuffer> vertices = nullptr;
    id<MTLBuffer> indices = nullptr;
    id<MTLBuffer> texels = nullptr;
    u32 texel_buffer_offset0;
    u32 texel_buffer_offset1;
    PerfQueryGroup perf_query_group = static_cast<PerfQueryGroup>(-1);
    u32 perf_query_id;
  } m_state;

  u32 m_perf_query_tracker_counter = 0;
  bool m_manual_buffer_upload = false;

  void SetManualBufferUpload(bool enable);
  std::shared_ptr<PerfQueryTracker> NewPerfQueryTracker();
  void SetSamplerForce(u32 idx, const SamplerState& sampler);
  void Sync(BufferPair& buffer);
  Map CommitPreallocation(UploadBuffer buffer_idx, size_t actual_amt);
  void CheckViewport();
  void CheckScissor();
  void PrepareRender();
  void PrepareCompute();
};

extern std::unique_ptr<StateTracker> g_state_tracker;
}  // namespace Metal
