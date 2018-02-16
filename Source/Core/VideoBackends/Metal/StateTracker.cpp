// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/StateTracker.h"

#include <cstring>

#include "Common/Align.h"
#include "Common/Assert.h"

#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/Common.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "VideoBackends/Metal/MetalVertexFormat.h"
#include "VideoBackends/Metal/StreamBuffer.h"
#include "VideoBackends/Metal/Util.h"

#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace Metal
{
std::unique_ptr<StateTracker> g_state_tracker;

void StateTracker::SetFramebuffer(const MetalFramebuffer* fb)
{
  if (m_framebuffer == fb)
    return;

  EndRenderPass();
  m_framebuffer = fb;
}

void StateTracker::SetPipeline(const MetalPipeline* pipeline)
{
  if (m_pipeline == pipeline)
    return;

  if (!m_pipeline || !pipeline)
  {
    m_dirty_flags |= DIRTY_FLAG_DEPTH_STATE | DIRTY_FLAG_DEPTH_CLIP_MODE | DIRTY_FLAG_CULL_MODE;
  }
  else
  {
    if (m_pipeline->GetDepthStencilState().GetPtr() != pipeline->GetDepthStencilState().GetPtr())
      m_dirty_flags |= DIRTY_FLAG_DEPTH_STATE;
    if (m_pipeline->GetDepthClipMode() != pipeline->GetDepthClipMode())
      m_dirty_flags |= DIRTY_FLAG_DEPTH_CLIP_MODE;
    if (m_pipeline->GetCullMode() != pipeline->GetCullMode())
      m_dirty_flags |= DIRTY_FLAG_CULL_MODE;
  }

  m_pipeline = pipeline;
  m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

void StateTracker::SetVertexBuffer(const mtlpp::Buffer& buffer, u32 offset)
{
  if (m_vertex_buffer.GetPtr() != buffer.GetPtr())
    m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
  else if (m_vertex_buffer_offset != offset)
    m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER_OFFSET;
  else
    return;

  m_vertex_buffer = buffer;
  m_vertex_buffer_offset = offset;
}

void StateTracker::SetIndexBuffer(const mtlpp::Buffer& buffer, u32 offset, mtlpp::IndexType type)
{
  if (m_index_buffer.GetPtr() == buffer.GetPtr() && m_index_buffer_offset == offset &&
      m_index_type == type)
    return;

  m_index_buffer = buffer;
  m_index_buffer_offset = offset;
  m_index_type = type;
  m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void StateTracker::SetVertexUniforms(const mtlpp::Buffer& buffer, u32 offset)
{
  if (m_vertex_uniforms.buffer.GetPtr() != buffer.GetPtr())
    m_dirty_flags |= DIRTY_FLAG_VS_UBO;
  else if (m_vertex_uniforms.offset != offset)
    m_dirty_flags |= DIRTY_FLAG_VS_UBO_OFFSET;
  else
    return;

  m_vertex_uniforms.buffer = buffer;
  m_vertex_uniforms.offset = offset;
}

void StateTracker::SetPixelUniforms(u32 index, const mtlpp::Buffer& buffer, u32 offset)
{
  if (m_pixel_uniforms[index].buffer.GetPtr() != buffer.GetPtr())
    m_dirty_flags |= DIRTY_FLAG_PS_UBO << index;
  else if (m_pixel_uniforms[index].offset != offset)
    m_dirty_flags |= DIRTY_FLAG_PS_UBO_OFFSET << index;
  else
    return;

  m_pixel_uniforms[index].buffer = buffer;
  m_pixel_uniforms[index].offset = offset;
}

void StateTracker::SetPixelTexture(u32 index, const MetalTexture* tex)
{
  if (m_ps_textures[index] == tex)
    return;

  m_ps_textures[index] = tex;
  if (tex)
    m_ps_texture_objects[index] = tex->GetTexture();
  else
    m_ps_texture_objects[index] = {};
  m_dirty_flags |= DIRTY_FLAG_PS_TEXTURE0 << index;
}

void StateTracker::SetPixelSampler(u32 index, const SamplerState& ss)
{
  if (m_ps_samplers[index].hex == ss.hex)
    return;

  m_ps_samplers[index].hex = ss.hex;
  m_ps_sampler_objects[index] = g_metal_context->GetSamplerState(ss);
  m_dirty_flags |= DIRTY_FLAG_PS_SAMPLER0 << index;
}

void StateTracker::SetPixelSampler(u32 index, const mtlpp::SamplerState& ss)
{
  if (m_ps_sampler_objects[index].GetPtr() == ss.GetPtr())
    return;

  m_ps_sampler_objects[index] = ss;
  m_dirty_flags |= DIRTY_FLAG_PS_SAMPLER0 << index;
}

void StateTracker::UnbindTexture(const MetalTexture* tex)
{
  for (u32 i = 0; i < MAX_TEXTURE_BINDINGS; i++)
  {
    if (m_ps_textures[i] == tex)
    {
      m_ps_textures[i] = nullptr;
      m_ps_texture_objects[i] = {};
      m_dirty_flags |= DIRTY_FLAG_PS_TEXTURE0 << i;
    }
  }
}

void StateTracker::SetViewport(const mtlpp::Viewport& viewport)
{
  if (std::tie(viewport.OriginX, viewport.OriginY, viewport.Width, viewport.Height, viewport.ZNear,
               viewport.ZFar) == std::tie(m_viewport.OriginX, m_viewport.OriginY, m_viewport.Width,
                                          m_viewport.Height, m_viewport.ZNear, m_viewport.ZFar))
  {
    return;
  }

  m_viewport = viewport;
  m_dirty_flags |= DIRTY_FLAG_VIEWPORT;
}

void StateTracker::SetScissor(const mtlpp::ScissorRect& scissor)
{
  if (std::tie(scissor.X, scissor.Y, scissor.Width, scissor.Height) ==
      std::tie(m_scissor.X, m_scissor.Y, m_scissor.Width, m_scissor.Height))
  {
    return;
  }

  m_scissor = scissor;
  m_dirty_flags |= DIRTY_FLAG_SCISSOR;
}

void StateTracker::SetViewportAndScissor(u32 x, u32 y, u32 width, u32 height, float near, float far)
{
  mtlpp::Viewport vp;
  vp.OriginX = static_cast<float>(x);
  vp.OriginY = static_cast<float>(y);
  vp.Width = static_cast<float>(width);
  vp.Height = static_cast<float>(height);
  vp.ZNear = near;
  vp.ZFar = far;
  SetViewport(vp);

  mtlpp::ScissorRect sr;
  sr.X = x;
  sr.Y = y;
  sr.Width = width;
  sr.Height = height;
  SetScissor(sr);
}

void StateTracker::BeginRenderPass(mtlpp::LoadAction load_action,
                                   const Renderer::ClearColor& clear_color, float clear_depth)
{
  if (InRenderPass())
    return;

  const mtlpp::RenderPassDescriptor& rpd =
      load_action == mtlpp::LoadAction::Clear ?
          m_framebuffer->GetClearDescriptor(clear_color, clear_depth) :
          (load_action == mtlpp::LoadAction::DontCare ? m_framebuffer->GetDiscardDescriptor() :
                                                        m_framebuffer->GetLoadDescriptor());
  m_command_encoder = g_command_buffer_mgr->GetCurrentBuffer().RenderCommandEncoder(rpd);
  if (!m_command_encoder)
  {
    PanicAlert("Failed to get render command encoder.");
    return;
  }

  SetPendingRebind();
}

void StateTracker::EndRenderPass()
{
  if (!InRenderPass())
    return;

  m_command_encoder.EndEncoding();
  m_command_encoder = {};
}

void StateTracker::Bind(bool rebind_all)
{
  _dbg_assert_(VIDEO, m_pipeline != nullptr);
  if (!InRenderPass())
    BeginRenderPass();

  const u32 df = m_dirty_flags;
  m_dirty_flags = 0;

  if (m_vertex_uniforms.buffer)
  {
    if (df & DIRTY_FLAG_VS_UBO)
    {
      m_command_encoder.SetVertexBuffer(m_vertex_uniforms.buffer, m_vertex_uniforms.offset,
                                        FIRST_VERTEX_UBO_INDEX);
    }
    else if (df & DIRTY_FLAG_VS_UBO_OFFSET)
    {
      m_command_encoder.SetVertexBufferOffset(m_vertex_uniforms.offset, FIRST_VERTEX_UBO_INDEX);
    }
  }

  for (u32 i = 0; i < NUM_PIXEL_SHADER_UBOS; i++)
  {
    if (m_pixel_uniforms[i].buffer)
    {
      if (df & (DIRTY_FLAG_PS_UBO << i))
      {
        m_command_encoder.SetFragmentBuffer(m_pixel_uniforms[i].buffer, m_pixel_uniforms[i].offset,
                                            FIRST_PIXEL_UBO_INDEX + i);
      }
      else if (df & (DIRTY_FLAG_PS_UBO_OFFSET << i))
      {
        m_command_encoder.SetFragmentBufferOffset(m_pixel_uniforms[i].offset,
                                                  FIRST_PIXEL_UBO_INDEX + i);
      }
    }
  }

  if (df & DIRTY_FLAG_PS_TEXTURES)
  {
    ns::Range range(0, 0);
    for (u32 i = 0; i < MAX_TEXTURE_BINDINGS; i++)
    {
      if (m_ps_textures[i] && (df & (DIRTY_FLAG_PS_TEXTURE0 << i)))
      {
        range.Length++;
      }
      else
      {
        if (range.Length > 0)
        {
          m_command_encoder.SetFragmentTextures(&m_ps_texture_objects[range.Location], range);
          range.Length = 0;
        }
        range.Location = i + 1;
      }
    }
    if (range.Length > 0)
      m_command_encoder.SetFragmentTextures(&m_ps_texture_objects[range.Location], range);
  }

  if (df & DIRTY_FLAG_PS_SAMPLERS)
  {
    ns::Range range(0, 0);
    for (u32 i = 0; i < MAX_TEXTURE_BINDINGS; i++)
    {
      if (df & (DIRTY_FLAG_PS_SAMPLER0 << i))
      {
        range.Length++;
      }
      else
      {
        if (range.Length > 0)
        {
          m_command_encoder.SetFragmentSamplerStates(&m_ps_sampler_objects[range.Location], range);
          range.Length = 0;
        }
        range.Location = i + 1;
      }
    }
    if (range.Length > 0)
      m_command_encoder.SetFragmentSamplerStates(&m_ps_sampler_objects[range.Location], range);
  }

  if (m_vertex_buffer)
  {
    if (df & DIRTY_FLAG_VERTEX_BUFFER)
      m_command_encoder.SetVertexBuffer(m_vertex_buffer, m_vertex_buffer_offset, 0);
    else if (df & DIRTY_FLAG_VERTEX_BUFFER_OFFSET)
      m_command_encoder.SetVertexBufferOffset(m_vertex_buffer_offset, 0);
  }

  if (df & DIRTY_FLAG_VIEWPORT)
    m_command_encoder.SetViewport(m_viewport);

  if (df & DIRTY_FLAG_SCISSOR)
    m_command_encoder.SetScissorRect(m_scissor);

  if (df & DIRTY_FLAG_PIPELINE)
    m_command_encoder.SetRenderPipelineState(m_pipeline->GetRenderPipelineState());

  if (df & DIRTY_FLAG_DEPTH_STATE)
    m_command_encoder.SetDepthStencilState(m_pipeline->GetDepthStencilState());

  if (df & DIRTY_FLAG_DEPTH_CLIP_MODE)
    m_command_encoder.SetDepthClipMode(m_pipeline->GetDepthClipMode());

  if (df & DIRTY_FLAG_CULL_MODE)
    m_command_encoder.SetCullMode(m_pipeline->GetCullMode());
}

void StateTracker::Draw(u32 vertex_count, u32 base_vertex)
{
  Bind();
  m_command_encoder.Draw(m_pipeline->GetPrimitiveType(), base_vertex, vertex_count);
}

void StateTracker::DrawIndexed(u32 index_count, u32 base_vertex)
{
  Bind();
  if (base_vertex > 0)
  {
    m_command_encoder.DrawIndexed(m_pipeline->GetPrimitiveType(), index_count, m_index_type,
                                  m_index_buffer, m_index_buffer_offset, 1, base_vertex, 0);
  }
  else
  {
    m_command_encoder.DrawIndexed(m_pipeline->GetPrimitiveType(), index_count, m_index_type,
                                  m_index_buffer, m_index_buffer_offset);
  }
}

void StateTracker::SetPendingRebind()
{
  m_dirty_flags = DIRTY_FLAG_ALL;
}
}  // namespace Metal
