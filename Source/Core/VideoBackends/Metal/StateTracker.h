// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/LinearDiskCache.h"
#include "VideoBackends/Metal/Common.h"
#include "VideoBackends/Metal/MetalPipeline.h"
#include "VideoBackends/Metal/MetalTexture.h"
#include "VideoBackends/Metal/MetalVertexFormat.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Metal
{
class StateTracker
{
public:
  StateTracker() = default;
  ~StateTracker() = default;

  const MetalFramebuffer* GetFramebuffer() const { return m_framebuffer; }
  bool InRenderPass() const { return static_cast<bool>(m_command_encoder); }
  mtlpp::RenderCommandEncoder& GetCommandEncoder() { return m_command_encoder; }
  bool Initialize();

  void SetFramebuffer(const MetalFramebuffer* fb);

  void SetPipeline(const MetalPipeline* pipeline);
  void SetVertexBuffer(const mtlpp::Buffer& buffer, u32 offset);
  void SetIndexBuffer(const mtlpp::Buffer& buffer, u32 offset, mtlpp::IndexType type);

  void SetVertexUniforms(const mtlpp::Buffer& buffer, u32 offset);
  void SetPixelUniforms(u32 index, const mtlpp::Buffer& buffer, u32 offset);

  void SetPixelTexture(u32 index, const MetalTexture* tex);
  void SetPixelSampler(u32 index, const SamplerState& ss);
  void SetPixelSampler(u32 index, const mtlpp::SamplerState& ss);
  void UnbindTexture(const MetalTexture* tex);

  void SetViewport(const mtlpp::Viewport& viewport);
  void SetScissor(const mtlpp::ScissorRect& scissor);
  void SetViewportAndScissor(u32 x, u32 y, u32 width, u32 height, float near = 0.0f,
                             float far = 1.0f);

  void BeginRenderPass(mtlpp::LoadAction load_action = mtlpp::LoadAction::Load,
                       const Renderer::ClearColor& clear_color = {}, float clear_depth = 1.0f);
  void EndRenderPass();

  void Bind(bool rebind_all = false);
  void Draw(u32 vertex_count, u32 base_vertex = 0);
  void DrawIndexed(u32 index_count, u32 base_vertex = 0);

  // Set dirty flags on everything to force re-bind at next draw time.
  void SetPendingRebind();

private:
  enum DITRY_FLAG : u64
  {
    DIRTY_FLAG_PS_TEXTURE0 = (1 << 0),
    DIRTY_FLAG_PS_TEXTURE1 = (1 << 1),
    DIRTY_FLAG_PS_TEXTURE2 = (1 << 2),
    DIRTY_FLAG_PS_TEXTURE3 = (1 << 3),
    DIRTY_FLAG_PS_TEXTURE4 = (1 << 4),
    DIRTY_FLAG_PS_TEXTURE5 = (1 << 5),
    DIRTY_FLAG_PS_TEXTURE6 = (1 << 6),
    DIRTY_FLAG_PS_TEXTURE7 = (1 << 7),
    DIRTY_FLAG_PS_TEXTURES = (255 << 0),
    DIRTY_FLAG_PS_SAMPLER0 = (1 << 8),
    DIRTY_FLAG_PS_SAMPLER1 = (1 << 9),
    DIRTY_FLAG_PS_SAMPLER2 = (1 << 10),
    DIRTY_FLAG_PS_SAMPLER3 = (1 << 11),
    DIRTY_FLAG_PS_SAMPLER4 = (1 << 12),
    DIRTY_FLAG_PS_SAMPLER5 = (1 << 13),
    DIRTY_FLAG_PS_SAMPLER6 = (1 << 14),
    DIRTY_FLAG_PS_SAMPLER7 = (1 << 15),
    DIRTY_FLAG_PS_SAMPLERS = (255 << 8),
    DIRTY_FLAG_VS_UBO = (1 << 16),
    DIRTY_FLAG_PS_UBO = (1 << 17),
    DIRTY_FLAG_PS_UBO2 = (1 << 18),
    DIRTY_FLAG_VS_UBO_OFFSET = (1 << 19),
    DIRTY_FLAG_PS_UBO_OFFSET = (1 << 20),
    DIRTY_FLAG_PS_UBO2_OFFSET = (1 << 21),
    DIRTY_FLAG_VERTEX_BUFFER = (1 << 22),
    DIRTY_FLAG_VERTEX_BUFFER_OFFSET = (1 << 23),
    DIRTY_FLAG_INDEX_BUFFER = (1 << 24),
    DIRTY_FLAG_VIEWPORT = (1 << 25),
    DIRTY_FLAG_SCISSOR = (1 << 26),
    DIRTY_FLAG_PIPELINE = (1 << 27),
    DIRTY_FLAG_DEPTH_STATE = (1 << 28),
    DIRTY_FLAG_DEPTH_CLIP_MODE = (1 << 29),
    DIRTY_FLAG_CULL_MODE = (1 << 30),

    DIRTY_FLAG_ALL = 0xffffffffffffffffULL
  };

  // Which bindings/state has to be updated before the next draw.
  u64 m_dirty_flags = DIRTY_FLAG_ALL;

  // input assembly
  mtlpp::Buffer m_vertex_buffer;
  u32 m_vertex_buffer_offset = 0;
  mtlpp::Buffer m_index_buffer;
  u32 m_index_buffer_offset = 0;
  mtlpp::IndexType m_index_type = mtlpp::IndexType::UInt16;

  // state objects
  const MetalPipeline* m_pipeline;

  // shader bindings
  std::array<const MetalTexture*, MAX_TEXTURE_BINDINGS> m_ps_textures;
  std::array<SamplerState, MAX_TEXTURE_BINDINGS> m_ps_samplers;
  std::array<mtlpp::Texture, MAX_TEXTURE_BINDINGS> m_ps_texture_objects;
  std::array<mtlpp::SamplerState, MAX_TEXTURE_BINDINGS> m_ps_sampler_objects;

  // rasterization
  mtlpp::Viewport m_viewport = {0.0, 0.0, 1.0, 1.0, 0.0, 1.0};
  mtlpp::ScissorRect m_scissor = {0, 0, 1, 1};

  // uniform buffers
  struct UniformBufferBinding
  {
    mtlpp::Buffer buffer;
    u32 offset;
  };
  UniformBufferBinding m_vertex_uniforms;
  std::array<UniformBufferBinding, NUM_PIXEL_SHADER_UBOS> m_pixel_uniforms;

  // destination
  const MetalFramebuffer* m_framebuffer = nullptr;
  mtlpp::RenderCommandEncoder m_command_encoder;
};

extern std::unique_ptr<StateTracker> g_state_tracker;
}  // namespace Metal
