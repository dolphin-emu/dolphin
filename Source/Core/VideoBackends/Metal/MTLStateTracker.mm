// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLStateTracker.h"

#include <algorithm>
#include <bit>
#include <mutex>

#include "Common/Assert.h"

#include "Core/System.h"

#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLPerfQuery.h"
#include "VideoBackends/Metal/MTLPipeline.h"
#include "VideoBackends/Metal/MTLTexture.h"
#include "VideoBackends/Metal/MTLUtil.h"

#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

static constexpr u32 PERF_QUERY_BUFFER_SIZE = 512;

std::unique_ptr<Metal::StateTracker> Metal::g_state_tracker;

struct Metal::StateTracker::Backref
{
  std::mutex mtx;
  StateTracker* state_tracker;
  explicit Backref(StateTracker* state_tracker) : state_tracker(state_tracker) {}
};

struct Metal::StateTracker::PerfQueryTracker
{
  MRCOwned<id<MTLBuffer>> buffer;
  const u64* contents;
  std::vector<PerfQueryGroup> groups;
  u32 query_id;
};

static NSString* GetName(Metal::StateTracker::UploadBuffer buffer)
{
  // clang-format off
  switch (buffer)
  {
    case Metal::StateTracker::UploadBuffer::Texels:  return @"Texels";
    case Metal::StateTracker::UploadBuffer::Vertex:  return @"Vertices";
    case Metal::StateTracker::UploadBuffer::Index:   return @"Indices";
    case Metal::StateTracker::UploadBuffer::Uniform: return @"Uniforms";
    case Metal::StateTracker::UploadBuffer::Other:   return @"Generic Upload";
  }
  // clang-format on
}

// MARK: - UsageTracker

bool Metal::StateTracker::UsageTracker::PrepareForAllocation(u64 last_draw, size_t amt)
{
  auto removeme = std::find_if(m_usage.begin(), m_usage.end(),
                               [last_draw](UsageEntry usage) { return usage.drawno > last_draw; });
  if (removeme != m_usage.begin())
    m_usage.erase(m_usage.begin(), removeme);

  bool still_in_use = false;
  const bool needs_wrap = m_pos + amt > m_size;
  if (!m_usage.empty())
  {
    size_t used = m_usage.front().pos;
    if (needs_wrap)
      still_in_use = used >= m_pos || used < amt;
    else
      still_in_use = used >= m_pos && used < m_pos + amt;
  }
  if (needs_wrap)
    m_pos = 0;

  return still_in_use || amt > m_size;
}

size_t Metal::StateTracker::UsageTracker::Allocate(u64 current_draw, size_t amt)
{
  // Allocation of zero bytes would make the buffer think it's full
  // Zero bytes is useless anyways, so don't mark usage in that case
  if (!amt)
    return m_pos;
  if (m_usage.empty() || m_usage.back().drawno != current_draw)
    m_usage.push_back({current_draw, m_pos});
  size_t ret = m_pos;
  m_pos += amt;
  return ret;
}

void Metal::StateTracker::UsageTracker::Reset(size_t new_size)
{
  m_usage.clear();
  m_size = new_size;
  m_pos = 0;
}

// MARK: - StateTracker

Metal::StateTracker::StateTracker() : m_backref(std::make_shared<Backref>(this))
{
  m_flags.should_apply_label = true;
  m_fence = MRCTransfer([g_device newFence]);
  for (MRCOwned<MTLRenderPassDescriptor*>& rpdesc : m_render_pass_desc)
  {
    rpdesc = MRCTransfer([MTLRenderPassDescriptor new]);
    [[rpdesc depthAttachment] setStoreAction:MTLStoreActionStore];
    [[rpdesc stencilAttachment] setStoreAction:MTLStoreActionStore];
  }
  m_resolve_pass_desc = MRCTransfer([MTLRenderPassDescriptor new]);
  auto color0 = [[m_resolve_pass_desc colorAttachments] objectAtIndexedSubscript:0];
  [color0 setLoadAction:MTLLoadActionLoad];
  [color0 setStoreAction:MTLStoreActionMultisampleResolve];
  MTLTextureDescriptor* texdesc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:1
                                                        height:1
                                                     mipmapped:NO];
  [texdesc setTextureType:MTLTextureType2DArray];
  [texdesc setUsage:MTLTextureUsageShaderRead];
  [texdesc setStorageMode:MTLStorageModePrivate];
  m_dummy_texture = MRCTransfer([g_device newTextureWithDescriptor:texdesc]);
  [m_dummy_texture setLabel:@"Dummy Texture"];
  for (size_t i = 0; i < std::size(m_state.samplers); ++i)
  {
    SetSamplerForce(i, RenderState::GetLinearSamplerState());
    SetTexture(i, m_dummy_texture);
  }
}

Metal::StateTracker::~StateTracker()
{
  FlushEncoders();
  std::lock_guard<std::mutex> lock(m_backref->mtx);
  m_backref->state_tracker = nullptr;
}

// MARK: BufferPair Ops

Metal::StateTracker::Map Metal::StateTracker::AllocateForTextureUpload(size_t amt)
{
  amt = (amt + 15) & ~15ull;
  CPUBuffer& buffer = m_texture_upload_buffer;
  u64 last_draw = m_last_finished_draw.load(std::memory_order_acquire);
  bool needs_new = buffer.usage.PrepareForAllocation(last_draw, amt);
  if (__builtin_expect(needs_new, false))
  {
    // Orphan buffer
    size_t newsize = std::max<size_t>(buffer.usage.Size() * 2, 4096);
    while (newsize < amt)
      newsize *= 2;
    MTLResourceOptions options =
        MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
    buffer.mtlbuffer = MRCTransfer([g_device newBufferWithLength:newsize options:options]);
    [buffer.mtlbuffer setLabel:@"Texture Upload Buffer"];
    ASSERT_MSG(VIDEO, buffer.mtlbuffer, "Failed to allocate MTLBuffer (out of memory?)");
    buffer.buffer = [buffer.mtlbuffer contents];
    buffer.usage.Reset(newsize);
  }

  size_t pos = buffer.usage.Allocate(m_current_draw, amt);

  Map ret = {buffer.mtlbuffer, pos, reinterpret_cast<char*>(buffer.buffer) + pos};
  DEBUG_ASSERT(pos <= buffer.usage.Size() &&
               "Previous code should have guaranteed there was enough space");
  return ret;
}

std::pair<void*, size_t> Metal::StateTracker::Preallocate(UploadBuffer buffer_idx, size_t amt)
{
  BufferPair& buffer = m_upload_buffers[static_cast<int>(buffer_idx)];
  u64 last_draw = m_last_finished_draw.load(std::memory_order_acquire);
  size_t base_pos = buffer.usage.Pos();
  bool needs_new = buffer.usage.PrepareForAllocation(last_draw, amt);
  bool needs_upload = needs_new || buffer.usage.Pos() == 0;
  if (m_manual_buffer_upload && needs_upload)
  {
    if (base_pos != buffer.last_upload)
    {
      id<MTLBlitCommandEncoder> encoder = GetUploadEncoder();
      [encoder copyFromBuffer:buffer.cpubuffer
                 sourceOffset:buffer.last_upload
                     toBuffer:buffer.gpubuffer
            destinationOffset:buffer.last_upload
                         size:base_pos - buffer.last_upload];
    }
    buffer.last_upload = 0;
  }
  if (__builtin_expect(needs_new, false))
  {
    // Orphan buffer
    size_t newsize = std::max<size_t>(buffer.usage.Size() * 2, 4096);
    while (newsize < amt)
      newsize *= 2;
    MTLResourceOptions options =
        MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
    buffer.cpubuffer = MRCTransfer([g_device newBufferWithLength:newsize options:options]);
    [buffer.cpubuffer setLabel:GetName(buffer_idx)];
    ASSERT_MSG(VIDEO, buffer.cpubuffer, "Failed to allocate MTLBuffer (out of memory?)");
    buffer.buffer = [buffer.cpubuffer contents];
    buffer.usage.Reset(newsize);
    if (g_features.manual_buffer_upload)
    {
      options = MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked;
      buffer.gpubuffer = MRCTransfer([g_device newBufferWithLength:newsize options:options]);
      [buffer.gpubuffer setLabel:GetName(buffer_idx)];
      ASSERT_MSG(VIDEO, buffer.gpubuffer, "Failed to allocate MTLBuffer (out of memory?)");
    }
  }
  size_t pos = buffer.usage.Pos();
  return std::make_pair(reinterpret_cast<char*>(buffer.buffer) + pos, pos);
}

Metal::StateTracker::Map Metal::StateTracker::CommitPreallocation(UploadBuffer buffer_idx,
                                                                  size_t amt)
{
  BufferPair& buffer = m_upload_buffers[static_cast<int>(buffer_idx)];
  size_t pos = buffer.usage.Allocate(m_current_draw, amt);
  Map ret = {nil, pos, reinterpret_cast<char*>(buffer.buffer) + pos};
  ret.gpu_buffer = m_manual_buffer_upload ? buffer.gpubuffer : buffer.cpubuffer;
  DEBUG_ASSERT(pos <= buffer.usage.Size() &&
               "Previous code should have guaranteed there was enough space");
  return ret;
}

void Metal::StateTracker::Sync(BufferPair& buffer)
{
  if (!m_manual_buffer_upload || buffer.usage.Pos() == buffer.last_upload)
    return;

  id<MTLBlitCommandEncoder> encoder = GetUploadEncoder();
  [encoder copyFromBuffer:buffer.cpubuffer
             sourceOffset:buffer.last_upload
                 toBuffer:buffer.gpubuffer
        destinationOffset:buffer.last_upload
                     size:buffer.usage.Pos() - buffer.last_upload];
  buffer.last_upload = buffer.usage.Pos();
}

// MARK: Render Pass / Encoder Management

id<MTLBlitCommandEncoder> Metal::StateTracker::GetUploadEncoder()
{
  if (!m_upload_cmdbuf)
  {
    @autoreleasepool
    {
      m_upload_cmdbuf = MRCRetain([g_queue commandBuffer]);
      [m_upload_cmdbuf setLabel:@"Vertex Upload"];
      m_upload_encoder = MRCRetain([m_upload_cmdbuf blitCommandEncoder]);
      [m_upload_encoder setLabel:@"Vertex Upload"];
    }
  }
  return m_upload_encoder;
}

id<MTLBlitCommandEncoder> Metal::StateTracker::GetTextureUploadEncoder()
{
  if (!m_texture_upload_cmdbuf)
  {
    @autoreleasepool
    {
      m_texture_upload_cmdbuf = MRCRetain([g_queue commandBuffer]);
      [m_texture_upload_cmdbuf setLabel:@"Texture Upload"];
      m_texture_upload_encoder = MRCRetain([m_texture_upload_cmdbuf blitCommandEncoder]);
      [m_texture_upload_encoder setLabel:@"Texture Upload"];
    }
  }
  return m_texture_upload_encoder;
}

id<MTLCommandBuffer> Metal::StateTracker::GetRenderCmdBuf()
{
  if (!m_current_render_cmdbuf)
  {
    @autoreleasepool
    {
      m_current_render_cmdbuf = MRCRetain([g_queue commandBuffer]);
      [m_current_render_cmdbuf setLabel:@"Draw"];
    }
  }
  return m_current_render_cmdbuf;
}

void Metal::StateTracker::SetCurrentFramebuffer(Framebuffer* framebuffer)
{
  if (framebuffer == m_current_framebuffer)
    return;
  EndRenderPass();
  m_current_framebuffer = framebuffer;
}

MTLRenderPassDescriptor* Metal::StateTracker::GetRenderPassDescriptor(Framebuffer* framebuffer,
                                                                      MTLLoadAction load_action)
{
  const AbstractTextureFormat depth_fmt = framebuffer->GetDepthFormat();
  MTLRenderPassDescriptor* desc;
  if (depth_fmt == AbstractTextureFormat::Undefined)
    desc = m_render_pass_desc[0];
  else if (!Util::HasStencil(depth_fmt))
    desc = m_render_pass_desc[1];
  else
    desc = m_render_pass_desc[2];
  desc.colorAttachments[0].texture = framebuffer->GetColor();
  desc.colorAttachments[0].loadAction = load_action;
  if (depth_fmt != AbstractTextureFormat::Undefined)
  {
    desc.depthAttachment.texture = framebuffer->GetDepth();
    desc.depthAttachment.loadAction = load_action;
    if (Util::HasStencil(depth_fmt))
    {
      desc.stencilAttachment.texture = framebuffer->GetDepth();
      desc.stencilAttachment.loadAction = load_action;
    }
  }
  return desc;
}

void Metal::StateTracker::BeginClearRenderPass(MTLClearColor color, float depth)
{
  Framebuffer* framebuffer = m_current_framebuffer;
  MTLRenderPassDescriptor* desc = GetRenderPassDescriptor(framebuffer, MTLLoadActionClear);
  desc.colorAttachments[0].clearColor = color;
  if (framebuffer->GetDepthFormat() != AbstractTextureFormat::Undefined)
  {
    desc.depthAttachment.clearDepth = depth;
    if (Util::HasStencil(framebuffer->GetDepthFormat()))
      desc.stencilAttachment.clearStencil = 0;
  }
  BeginRenderPass(desc);
}

void Metal::StateTracker::BeginRenderPass(MTLLoadAction load_action)
{
  if (m_current_render_encoder)
    return;
  BeginRenderPass(GetRenderPassDescriptor(m_current_framebuffer, load_action));
}

void Metal::StateTracker::BeginRenderPass(MTLRenderPassDescriptor* descriptor)
{
  EndRenderPass();
  if (m_current_perf_query)
    [descriptor setVisibilityResultBuffer:m_current_perf_query->buffer];
  m_current_render_encoder =
      MRCRetain([GetRenderCmdBuf() renderCommandEncoderWithDescriptor:descriptor]);
  if (m_current_perf_query)
    [descriptor setVisibilityResultBuffer:nil];
  if (m_manual_buffer_upload)
    [m_current_render_encoder waitForFence:m_fence beforeStages:MTLRenderStageVertex];
  AbstractTexture* attachment = m_current_framebuffer->GetColorAttachment();
  if (!attachment)
    attachment = m_current_framebuffer->GetDepthAttachment();
  static_assert(std::is_trivially_copyable<decltype(m_current)>::value,
                "Make sure we can memset this");
  memset(&m_current, 0, sizeof(m_current));
  m_current.width = attachment->GetWidth();
  m_current.height = attachment->GetHeight();
  m_current.scissor_rect = MathUtil::Rectangle<int>(0, 0, m_current.width, m_current.height);
  m_current.viewport = {
      0.f, 0.f, static_cast<float>(m_current.width), static_cast<float>(m_current.height),
      0.f, 1.f};
  m_current.depth_stencil = DepthStencilSelector(false, CompareMode::Always);
  m_current.depth_clip_mode = MTLDepthClipModeClip;
  m_current.cull_mode = MTLCullModeNone;
  m_current.perf_query_group = static_cast<PerfQueryGroup>(-1);
  m_flags.NewEncoder();
  m_dirty_samplers = (1 << VideoCommon::MAX_PIXEL_SHADER_SAMPLERS) - 1;
  m_dirty_textures = (1 << VideoCommon::MAX_PIXEL_SHADER_SAMPLERS) - 1;
  CheckScissor();
  CheckViewport();
  ASSERT_MSG(VIDEO, m_current_render_encoder, "Failed to create render encoder!");
}

void Metal::StateTracker::BeginComputePass()
{
  EndRenderPass();
  m_current_compute_encoder = MRCRetain([GetRenderCmdBuf() computeCommandEncoder]);
  [m_current_compute_encoder setLabel:@"Compute"];
  if (m_manual_buffer_upload)
    [m_current_compute_encoder waitForFence:m_fence];
  m_flags.NewEncoder();
  m_dirty_samplers = (1 << VideoCommon::MAX_PIXEL_SHADER_SAMPLERS) - 1;
  m_dirty_textures = (1 << VideoCommon::MAX_PIXEL_SHADER_SAMPLERS) - 1;
}

void Metal::StateTracker::EndRenderPass()
{
  if (m_current_render_encoder)
  {
    if (m_flags.bbox_fence && m_state.bbox_download_fence)
      [m_current_render_encoder updateFence:m_state.bbox_download_fence
                                afterStages:MTLRenderStageFragment];
    [m_current_render_encoder endEncoding];
    m_current_render_encoder = nullptr;
  }
  if (m_current_compute_encoder)
  {
    [m_current_compute_encoder endEncoding];
    m_current_compute_encoder = nullptr;
  }
}

void Metal::StateTracker::FlushEncoders()
{
  if (!m_current_render_cmdbuf)
    return;
  EndRenderPass();
  for (int i = 0; i <= static_cast<int>(UploadBuffer::Last); ++i)
    Sync(m_upload_buffers[i]);
  if (!m_manual_buffer_upload)
  {
    ASSERT(!m_upload_cmdbuf && "Should never be used!");
  }
  else if (m_upload_cmdbuf)
  {
    [m_upload_encoder updateFence:m_fence];
    [m_upload_encoder endEncoding];
    [m_upload_cmdbuf commit];
    m_upload_encoder = nullptr;
    m_upload_cmdbuf = nullptr;
  }
  if (m_texture_upload_cmdbuf)
  {
    [m_texture_upload_encoder endEncoding];
    [m_texture_upload_cmdbuf commit];
    m_texture_upload_encoder = nullptr;
    m_texture_upload_cmdbuf = nullptr;
  }
  [m_current_render_cmdbuf
      addCompletedHandler:[backref = m_backref, draw = m_current_draw,
                           q = std::move(m_current_perf_query)](id<MTLCommandBuffer> buf) {
        std::lock_guard<std::mutex> guard(backref->mtx);
        if (StateTracker* tracker = backref->state_tracker)
        {
          // We can do the update non-atomically because we only ever update under the lock
          u64 newval =
              std::max(draw, tracker->m_last_finished_draw.load(std::memory_order_relaxed));
          tracker->m_last_finished_draw.store(newval, std::memory_order_release);
          if (q)
          {
            if (PerfQuery* query = static_cast<PerfQuery*>(g_perf_query.get()))
              query->ReturnResults(q->contents, q->groups.data(), q->groups.size(), q->query_id);
            tracker->m_perf_query_tracker_cache.emplace_back(std::move(q));
          }
        }
      }];
  m_current_perf_query = nullptr;
  [m_current_render_cmdbuf commit];
  m_last_render_cmdbuf = std::move(m_current_render_cmdbuf);
  m_current_render_cmdbuf = nullptr;
  m_current_draw++;
  if (g_features.manual_buffer_upload && !m_manual_buffer_upload)
    SetManualBufferUpload(true);
}

void Metal::StateTracker::WaitForFlushedEncoders()
{
  [m_last_render_cmdbuf waitUntilCompleted];
}

void Metal::StateTracker::ReloadSamplers()
{
  for (size_t i = 0; i < std::size(m_state.samplers); ++i)
    m_state.samplers[i] = g_object_cache->GetSampler(m_state.sampler_states[i]);
}

void Metal::StateTracker::SetManualBufferUpload(bool enabled)
{
  // When a game does something that needs CPU-GPU sync (e.g. bbox, texture download, etc),
  // the next command buffer will be done with manual buffer upload disabled,
  // since overlapping the upload with the previous draw won't be possible (due to sync).
  // This greatly improves performance in heavy bbox games like Super Paper Mario.
  m_manual_buffer_upload = enabled;
  if (enabled)
  {
    for (BufferPair& buffer : m_upload_buffers)
    {
      // Update sync positions, since Sync doesn't do it when manual buffer upload is off
      buffer.last_upload = buffer.usage.Pos();
    }
  }
}

// MARK: State Setters

void Metal::StateTracker::SetPipeline(const Pipeline* pipe)
{
  if (pipe != m_state.render_pipeline)
  {
    m_state.render_pipeline = pipe;
    m_flags.has_pipeline = false;
  }
}

void Metal::StateTracker::SetPipeline(const ComputePipeline* pipe)
{
  if (pipe != m_state.compute_pipeline)
  {
    m_state.compute_pipeline = pipe;
    m_flags.has_pipeline = false;
  }
}

void Metal::StateTracker::SetScissor(const MathUtil::Rectangle<int>& rect)
{
  m_state.scissor_rect = rect;
  CheckScissor();
}

void Metal::StateTracker::CheckScissor()
{
  auto clipped = m_state.scissor_rect;
  clipped.ClampUL(0, 0, m_current.width, m_current.height);
  m_flags.has_scissor = clipped == m_current.scissor_rect;
}

void Metal::StateTracker::SetViewport(float x, float y, float width, float height, float near_depth,
                                      float far_depth)
{
  m_state.viewport = {x, y, width, height, near_depth, far_depth};
  CheckViewport();
}

void Metal::StateTracker::CheckViewport()
{
  m_flags.has_viewport =
      0 == memcmp(&m_state.viewport, &m_current.viewport, sizeof(m_current.viewport));
}

void Metal::StateTracker::SetTexture(u32 idx, id<MTLTexture> texture)
{
  ASSERT(idx < std::size(m_state.textures));
  if (!texture)
    texture = m_dummy_texture;
  if (m_state.textures[idx] != texture)
  {
    m_state.textures[idx] = texture;
    m_dirty_textures |= 1 << idx;
  }
}

void Metal::StateTracker::SetSamplerForce(u32 idx, const SamplerState& sampler)
{
  m_state.samplers[idx] = g_object_cache->GetSampler(sampler);
  m_state.sampler_min_lod[idx] = sampler.tm1.min_lod;
  m_state.sampler_max_lod[idx] = sampler.tm1.max_lod;
  m_state.sampler_states[idx] = sampler;
  m_dirty_samplers |= 1 << idx;
}

void Metal::StateTracker::SetSampler(u32 idx, const SamplerState& sampler)
{
  ASSERT(idx < std::size(m_state.samplers));
  if (m_state.sampler_states[idx] != sampler)
    SetSamplerForce(idx, sampler);
}

void Metal::StateTracker::SetComputeTexture(const Texture* texture)
{
  if (m_state.compute_texture != texture)
  {
    m_state.compute_texture = texture;
    m_flags.has_compute_texture = false;
  }
}

void Metal::StateTracker::UnbindTexture(id<MTLTexture> texture)
{
  for (size_t i = 0; i < std::size(m_state.textures); ++i)
  {
    if (m_state.textures[i] == texture)
    {
      m_state.textures[i] = m_dummy_texture;
      m_dirty_textures |= 1 << i;
    }
  }
}

void Metal::StateTracker::InvalidateUniforms(bool vertex, bool geometry, bool fragment)
{
  m_flags.has_gx_vs_uniform &= !vertex;
  m_flags.has_gx_gs_uniform &= !geometry;
  m_flags.has_gx_ps_uniform &= !fragment;
}

void Metal::StateTracker::SetUtilityUniform(const void* buffer, size_t size)
{
  if (m_state.utility_uniform_capacity < size)
  {
    m_state.utility_uniform = std::unique_ptr<u8[]>(new u8[size]);
    m_state.utility_uniform_capacity = size;
  }
  m_state.utility_uniform_size = size;
  memcpy(m_state.utility_uniform.get(), buffer, size);
  m_flags.has_utility_vs_uniform = false;
  m_flags.has_utility_ps_uniform = false;
}

void Metal::StateTracker::SetTexelBuffer(id<MTLBuffer> buffer, u32 offset0, u32 offset1)
{
  m_state.texels = buffer;
  m_state.texel_buffer_offset0 = offset0;
  m_state.texel_buffer_offset1 = offset1;
  m_flags.has_texel_buffer = false;
}

void Metal::StateTracker::SetVerticesAndIndices(id<MTLBuffer> vertices, id<MTLBuffer> indices)
{
  if (m_state.vertices != vertices)
  {
    m_flags.has_vertices = false;
    m_state.vertices = vertices;
  }
  m_state.indices = indices;
}

void Metal::StateTracker::SetBBoxBuffer(id<MTLBuffer> bbox, id<MTLFence> upload,
                                        id<MTLFence> download)
{
  m_state.bbox = bbox;
  m_state.bbox_upload_fence = upload;
  m_state.bbox_download_fence = download;
}

void Metal::StateTracker::SetVertexBufferNow(u32 idx, id<MTLBuffer> buffer, u32 offset)
{
  if (idx < std::size(m_current.vertex_buffers) && m_current.vertex_buffers[idx] == buffer)
  {
    [m_current_render_encoder setVertexBufferOffset:offset atIndex:idx];
  }
  else
  {
    [m_current_render_encoder setVertexBuffer:buffer offset:offset atIndex:idx];
    m_current.vertex_buffers[idx] = buffer;
  }
}

void Metal::StateTracker::SetFragmentBufferNow(u32 idx, id<MTLBuffer> buffer, u32 offset)
{
  if (idx < std::size(m_current.fragment_buffers) && m_current.fragment_buffers[idx] == buffer)
  {
    [m_current_render_encoder setFragmentBufferOffset:offset atIndex:idx];
  }
  else
  {
    [m_current_render_encoder setFragmentBuffer:buffer offset:offset atIndex:idx];
    m_current.fragment_buffers[idx] = buffer;
  }
}

std::shared_ptr<Metal::StateTracker::PerfQueryTracker> Metal::StateTracker::NewPerfQueryTracker()
{
  static_cast<PerfQuery*>(g_perf_query.get())->IncCount();
  // The cache is repopulated asynchronously
  std::lock_guard<std::mutex> lock(m_backref->mtx);
  if (m_perf_query_tracker_cache.empty())
  {
    // Make a new one
    @autoreleasepool
    {
      std::shared_ptr<PerfQueryTracker> tracker = std::make_shared<PerfQueryTracker>();
      const MTLResourceOptions options =
          MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked;
      id<MTLBuffer> buffer = [g_device newBufferWithLength:PERF_QUERY_BUFFER_SIZE * sizeof(u64)
                                                   options:options];
      [buffer setLabel:[NSString stringWithFormat:@"PerfQuery Buffer %d",
                                                  m_perf_query_tracker_counter++]];
      tracker->buffer = MRCTransfer(buffer);
      tracker->contents = static_cast<const u64*>([buffer contents]);
      tracker->query_id = m_state.perf_query_id;
      return tracker;
    }
  }
  else
  {
    // Reuse an old one
    std::shared_ptr<PerfQueryTracker> tracker = std::move(m_perf_query_tracker_cache.back());
    m_perf_query_tracker_cache.pop_back();
    tracker->groups.clear();
    tracker->query_id = m_state.perf_query_id;
    return tracker;
  }
}

void Metal::StateTracker::EnablePerfQuery(PerfQueryGroup group, u32 query_id)
{
  m_state.perf_query_group = group;
  m_state.perf_query_id = query_id;
  if (!m_current_perf_query || m_current_perf_query->query_id != query_id ||
      m_current_perf_query->groups.size() == PERF_QUERY_BUFFER_SIZE)
  {
    if (m_current_render_encoder)
      EndRenderPass();
    if (m_current_perf_query)
    {
      [m_current_render_cmdbuf
          addCompletedHandler:[backref = m_backref, q = std::move(m_current_perf_query)](id) {
            std::lock_guard<std::mutex> guard(backref->mtx);
            if (StateTracker* tracker = backref->state_tracker)
            {
              if (PerfQuery* query = static_cast<PerfQuery*>(g_perf_query.get()))
                query->ReturnResults(q->contents, q->groups.data(), q->groups.size(), q->query_id);
              tracker->m_perf_query_tracker_cache.emplace_back(std::move(q));
            }
          }];
      m_current_perf_query.reset();
    }
  }
}

void Metal::StateTracker::DisablePerfQuery()
{
  m_state.perf_query_group = static_cast<PerfQueryGroup>(-1);
}

// MARK: Render

// clang-format off
static constexpr NSString* LABEL_GX   = @"GX Draw";
static constexpr NSString* LABEL_UTIL = @"Utility Draw";
// clang-format on

static NSRange RangeOfBits(u32 value)
{
  ASSERT(value && "Value must be nonzero");
  int low = std::countr_zero(value);
  int high = 31 - std::countl_zero(value);
  return NSMakeRange(low, high + 1 - low);
}

void Metal::StateTracker::PrepareRender()
{
  // BeginRenderPass needs this
  if (m_state.perf_query_group != static_cast<PerfQueryGroup>(-1) && !m_current_perf_query)
    m_current_perf_query = NewPerfQueryTracker();
  if (!m_current_render_encoder)
    BeginRenderPass(MTLLoadActionLoad);
  id<MTLRenderCommandEncoder> enc = m_current_render_encoder;
  const Pipeline* pipe = m_state.render_pipeline;
  bool is_gx = pipe->Usage() != AbstractPipelineUsage::Utility;
  NSString* label = is_gx ? LABEL_GX : LABEL_UTIL;
  if (m_flags.should_apply_label && m_current.label != label)
  {
    m_current.label = label;
    [m_current_render_encoder setLabel:label];
  }
  if (!m_flags.has_pipeline)
  {
    m_flags.has_pipeline = true;
    if (pipe->Get() != m_current.pipeline)
    {
      m_current.pipeline = pipe->Get();
      [enc setRenderPipelineState:pipe->Get()];
    }
    if (pipe->Cull() != m_current.cull_mode)
    {
      m_current.cull_mode = pipe->Cull();
      [enc setCullMode:pipe->Cull()];
    }
    if (pipe->DepthStencil() != m_current.depth_stencil)
    {
      m_current.depth_stencil = pipe->DepthStencil();
      [enc setDepthStencilState:g_object_cache->GetDepthStencil(m_current.depth_stencil)];
    }
    MTLDepthClipMode clip = is_gx && g_ActiveConfig.backend_info.bSupportsDepthClamp ?
                                MTLDepthClipModeClamp :
                                MTLDepthClipModeClip;
    if (clip != m_current.depth_clip_mode)
    {
      m_current.depth_clip_mode = clip;
      [enc setDepthClipMode:clip];
    }
    if (is_gx && m_state.bbox_upload_fence && !m_flags.bbox_fence && pipe->UsesFragmentBuffer(2))
    {
      m_flags.bbox_fence = true;
      [enc waitForFence:m_state.bbox_upload_fence beforeStages:MTLRenderStageFragment];
      [enc setFragmentBuffer:m_state.bbox offset:0 atIndex:2];
    }
  }
  if (!m_flags.has_viewport)
  {
    m_flags.has_viewport = true;
    m_current.viewport = m_state.viewport;
    MTLViewport metal;
    metal.originX = m_state.viewport.x;
    metal.originY = m_state.viewport.y;
    metal.width = m_state.viewport.width;
    metal.height = m_state.viewport.height;
    metal.znear = m_state.viewport.near_depth;
    metal.zfar = m_state.viewport.far_depth;
    [enc setViewport:metal];
  }
  if (!m_flags.has_scissor)
  {
    m_flags.has_scissor = true;
    m_current.scissor_rect = m_state.scissor_rect;
    m_current.scissor_rect.ClampUL(0, 0, m_current.width, m_current.height);
    MTLScissorRect metal;
    metal.x = m_current.scissor_rect.left;
    metal.y = m_current.scissor_rect.top;
    metal.width = m_current.scissor_rect.right - m_current.scissor_rect.left;
    metal.height = m_current.scissor_rect.bottom - m_current.scissor_rect.top;
    [enc setScissorRect:metal];
  }
  if (!m_flags.has_vertices && pipe->UsesVertexBuffer(0))
  {
    m_flags.has_vertices = true;
    if (m_state.vertices)
      SetVertexBufferNow(0, m_state.vertices, 0);
  }
  if (u32 dirty = m_dirty_textures & pipe->GetTextures())
  {
    m_dirty_textures &= ~pipe->GetTextures();
    NSRange range = RangeOfBits(dirty);
    [enc setFragmentTextures:&m_state.textures[range.location] withRange:range];
  }
  if (u32 dirty = m_dirty_samplers & pipe->GetSamplers())
  {
    m_dirty_samplers &= ~pipe->GetSamplers();
    NSRange range = RangeOfBits(dirty);
    [enc setFragmentSamplerStates:&m_state.samplers[range.location]
                     lodMinClamps:&m_state.sampler_min_lod[range.location]
                     lodMaxClamps:&m_state.sampler_max_lod[range.location]
                        withRange:range];
  }
  if (m_state.perf_query_group != m_current.perf_query_group)
  {
    m_current.perf_query_group = m_state.perf_query_group;
    if (m_state.perf_query_group == static_cast<PerfQueryGroup>(-1))
    {
      [enc setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
    }
    else
    {
      [enc setVisibilityResultMode:MTLVisibilityResultModeCounting
                            offset:m_current_perf_query->groups.size() * 8];
      m_current_perf_query->groups.push_back(m_state.perf_query_group);
    }
  }
  if (is_gx)
  {
    // GX draw
    if (!m_flags.has_gx_vs_uniform)
    {
      m_flags.has_gx_vs_uniform = true;
      Map map = Allocate(UploadBuffer::Uniform, sizeof(VertexShaderConstants), AlignMask::Uniform);
      auto& system = Core::System::GetInstance();
      auto& vertex_shader_manager = system.GetVertexShaderManager();
      memcpy(map.cpu_buffer, &vertex_shader_manager.constants, sizeof(VertexShaderConstants));
      SetVertexBufferNow(1, map.gpu_buffer, map.gpu_offset);
      if (pipe->UsesFragmentBuffer(1))
        SetFragmentBufferNow(1, map.gpu_buffer, map.gpu_offset);
      ADDSTAT(g_stats.this_frame.bytes_uniform_streamed,
              Align(sizeof(VertexShaderConstants), AlignMask::Uniform));
    }
    if (!m_flags.has_gx_gs_uniform && pipe->UsesVertexBuffer(2))
    {
      m_flags.has_gx_gs_uniform = true;
      auto& system = Core::System::GetInstance();
      auto& geometry_shader_manager = system.GetGeometryShaderManager();
      [m_current_render_encoder setVertexBytes:&geometry_shader_manager.constants
                                        length:sizeof(GeometryShaderConstants)
                                       atIndex:2];
      ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, sizeof(GeometryShaderConstants));
    }
    if (!m_flags.has_gx_ps_uniform)
    {
      m_flags.has_gx_ps_uniform = true;
      Map map = Allocate(UploadBuffer::Uniform, sizeof(PixelShaderConstants), AlignMask::Uniform);
      auto& system = Core::System::GetInstance();
      auto& pixel_shader_manager = system.GetPixelShaderManager();
      memcpy(map.cpu_buffer, &pixel_shader_manager.constants, sizeof(PixelShaderConstants));
      SetFragmentBufferNow(0, map.gpu_buffer, map.gpu_offset);
      ADDSTAT(g_stats.this_frame.bytes_uniform_streamed,
              Align(sizeof(PixelShaderConstants), AlignMask::Uniform));
    }
  }
  else
  {
    // Utility draw
    if (!m_flags.has_utility_vs_uniform && pipe->UsesVertexBuffer(1))
    {
      m_flags.has_utility_vs_uniform = true;
      m_flags.has_gx_vs_uniform = false;
      [enc setVertexBytes:m_state.utility_uniform.get()
                   length:m_state.utility_uniform_size
                  atIndex:1];
    }
    if (!m_flags.has_utility_ps_uniform && pipe->UsesFragmentBuffer(0))
    {
      m_flags.has_utility_ps_uniform = true;
      m_flags.has_gx_ps_uniform = false;
      [enc setFragmentBytes:m_state.utility_uniform.get()
                     length:m_state.utility_uniform_size
                    atIndex:0];
    }
    if (!m_flags.has_texel_buffer && pipe->UsesFragmentBuffer(2))
    {
      m_flags.has_texel_buffer = true;
      SetFragmentBufferNow(2, m_state.texels, m_state.texel_buffer_offset0);
    }
  }
}

void Metal::StateTracker::PrepareCompute()
{
  if (!m_current_compute_encoder)
    BeginComputePass();
  id<MTLComputeCommandEncoder> enc = m_current_compute_encoder;
  const ComputePipeline* pipe = m_state.compute_pipeline;
  if (!m_flags.has_pipeline)
  {
    m_flags.has_pipeline = true;
    [enc setComputePipelineState:pipe->GetComputePipeline()];
  }
  if (!m_flags.has_compute_texture && pipe->UsesTexture(0))
  {
    m_flags.has_compute_texture = true;
    [enc setTexture:m_state.compute_texture->GetMTLTexture() atIndex:0];
  }
  // Compute and render can't happen at the same time, so just reuse one of the flags
  if (!m_flags.has_utility_vs_uniform && pipe->UsesBuffer(0))
  {
    m_flags.has_utility_vs_uniform = true;
    [enc setBytes:m_state.utility_uniform.get() length:m_state.utility_uniform_size atIndex:0];
  }
  if (!m_flags.has_texel_buffer && pipe->UsesBuffer(2))
  {
    m_flags.has_texel_buffer = true;
    [enc setBuffer:m_state.texels offset:m_state.texel_buffer_offset0 atIndex:2];
    if (pipe->UsesBuffer(3))
      [enc setBuffer:m_state.texels offset:m_state.texel_buffer_offset1 atIndex:3];
  }
}

void Metal::StateTracker::Draw(u32 base_vertex, u32 num_vertices)
{
  if (!num_vertices)
    return;
  PrepareRender();
  [m_current_render_encoder drawPrimitives:m_state.render_pipeline->Prim()
                               vertexStart:base_vertex
                               vertexCount:num_vertices];
}

void Metal::StateTracker::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  if (!num_indices)  // Happens in Metroid Prime, Metal API validation doesn't like this
    return;
  PrepareRender();
  [m_current_render_encoder drawIndexedPrimitives:m_state.render_pipeline->Prim()
                                       indexCount:num_indices
                                        indexType:MTLIndexTypeUInt16
                                      indexBuffer:m_state.indices
                                indexBufferOffset:base_index * sizeof(u16)
                                    instanceCount:1
                                       baseVertex:base_vertex
                                     baseInstance:0];
}

void Metal::StateTracker::DispatchComputeShader(u32 groupsize_x, u32 groupsize_y, u32 groupsize_z,
                                                u32 groups_x, u32 groups_y, u32 groups_z)
{
  PrepareCompute();
  [m_current_compute_encoder
       dispatchThreadgroups:MTLSizeMake(groups_x, groups_y, groups_z)
      threadsPerThreadgroup:MTLSizeMake(groupsize_x, groupsize_y, groupsize_z)];
}

void Metal::StateTracker::ResolveTexture(id<MTLTexture> src, id<MTLTexture> dst, u32 layer,
                                         u32 level)
{
  EndRenderPass();
  auto color0 = [[m_resolve_pass_desc colorAttachments] objectAtIndexedSubscript:0];
  [color0 setTexture:src];
  [color0 setResolveTexture:dst];
  [color0 setResolveSlice:layer];
  [color0 setResolveLevel:level];
  id<MTLRenderCommandEncoder> enc =
      [GetRenderCmdBuf() renderCommandEncoderWithDescriptor:m_resolve_pass_desc];
  [enc setLabel:@"Multisample Resolve"];
  [enc endEncoding];
}
