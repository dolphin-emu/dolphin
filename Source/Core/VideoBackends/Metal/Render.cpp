// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "VideoBackends/Metal/MetalPipeline.h"
#include "VideoBackends/Metal/MetalShader.h"
#include "VideoBackends/Metal/MetalTexture.h"
#include "VideoBackends/Metal/Render.h"
#include "VideoBackends/Metal/StateTracker.h"
#include "VideoBackends/Metal/StreamBuffer.h"
#include "VideoBackends/Metal/TextureCache.h"
#include "VideoBackends/Metal/Util.h"
#include "VideoBackends/Metal/VertexManager.h"

#include "Common/MsgHandler.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace Metal
{
// Init functions
Renderer::Renderer(std::unique_ptr<MetalFramebuffer> backbuffer)
    : ::Renderer(backbuffer ? backbuffer->GetWidth() : 1, backbuffer ? backbuffer->GetHeight() : 1,
                 backbuffer ? backbuffer->GetColorFormat() : AbstractTextureFormat::Undefined),
      m_backbuffer(std::move(backbuffer))
{
  UpdateActiveConfig();
}

Renderer::~Renderer() = default;

void Renderer::SubmitCommandBuffer(bool wait_for_completion)
{
  g_state_tracker->EndRenderPass();
  g_command_buffer_mgr->SubmitCommandBuffer(wait_for_completion);

  // All state must be re-applied.
  g_state_tracker->SetPendingRebind();

  // Cached uniforms may be part of an old buffer, so re-upload them later.
  VertexManager::GetInstance()->InvalidateUniforms();
}

bool Renderer::Initialize()
{
  if (!::Renderer::Initialize())
    return false;

  // Create utility vertex streaming buffer.
  m_utility_vertex_buffer = StreamBuffer::Create(UTILITY_VERTEX_STREAMING_BUFFER_SIZE,
                                                 UTILITY_VERTEX_STREAMING_BUFFER_SIZE);
  if (!m_utility_vertex_buffer)
  {
    PanicAlert("Failed to create utility vertex streaming buffer.");
    return false;
  }

  return true;
}

void Renderer::Shutdown()
{
  g_state_tracker->EndRenderPass();
  ::Renderer::Shutdown();
}

std::unique_ptr<AbstractTexture> Renderer::CreateTexture(const TextureConfig& config)
{
  return MetalTexture::Create(config);
}

std::unique_ptr<AbstractStagingTexture> Renderer::CreateStagingTexture(StagingTextureType type,
                                                                       const TextureConfig& config)
{
  return MetalStagingTexture::Create(type, config);
}

std::unique_ptr<AbstractFramebuffer>
Renderer::CreateFramebuffer(const AbstractTexture* color_attachment,
                            const AbstractTexture* depth_attachment)
{
  return MetalFramebuffer::CreateForAbstractTexture(
      static_cast<const MetalTexture*>(color_attachment),
      static_cast<const MetalTexture*>(depth_attachment));
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromSource(ShaderStage stage,
                                                                 const char* source, size_t length)
{
  std::string source_str = std::string(source, length);
  return MetalShader::CreateFromSource(stage, source_str);
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromBinary(ShaderStage stage,
                                                                 const void* data, size_t length)
{
  return MetalShader::CreateFromBinary(stage, data, length);
}

std::unique_ptr<AbstractPipeline> Renderer::CreatePipeline(const AbstractPipelineConfig& config)
{
  return MetalPipeline::Create(config);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = EFBToScaledX(rc.left);
  result.top = EFBToScaledY(rc.top);
  result.right = EFBToScaledX(rc.right);
  result.bottom = EFBToScaledY(rc.bottom);
  return result;
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                           u32 color, u32 z)
{
  // Convert Z from 24-bit to floating-point.
  float depth = static_cast<float>(z & 0xFFFFFF) / 16777216.0f;
  if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
    depth = 1.0f - depth;

  // Byteswap BGRA->RGBA.
  color = ((color >> 16) & 0xFF) | (((color >> 8) & 0xFF) << 8) | ((color & 0xFF) << 16) |
          (((color >> 24) & 0xFF) << 24);

  TargetRectangle target_rc = ConvertEFBRectangle(rc);
  target_rc.ClampUL(0, 0, m_target_width, m_target_height);

  ResetAPIState();
  DrawClearQuad(target_rc, colorEnable, alphaEnable, zEnable, color, depth);
  RestoreAPIState();
}

void Renderer::SetTexture(u32 index, const AbstractTexture* texture)
{
  g_state_tracker->SetPixelTexture(index, static_cast<const MetalTexture*>(texture));
}

void Renderer::SetSamplerState(u32 index, const SamplerState& state)
{
  g_state_tracker->SetPixelSampler(index, state);
}

void Renderer::SetPipeline(const AbstractPipeline* pipeline)
{
  g_state_tracker->SetPipeline(static_cast<const MetalPipeline*>(pipeline));
}

void Renderer::SetFramebuffer(const AbstractFramebuffer* framebuffer)
{
  if (g_state_tracker->GetFramebuffer() == framebuffer)
    return;

  g_state_tracker->SetFramebuffer(static_cast<const MetalFramebuffer*>(framebuffer));
  g_state_tracker->BeginRenderPass(mtlpp::LoadAction::Load);
}

void Renderer::SetAndDiscardFramebuffer(const AbstractFramebuffer* framebuffer)
{
  if (g_state_tracker->GetFramebuffer() == framebuffer)
    return;

  g_state_tracker->SetFramebuffer(static_cast<const MetalFramebuffer*>(framebuffer));
  g_state_tracker->BeginRenderPass(mtlpp::LoadAction::DontCare);
}

void Renderer::SetAndClearFramebuffer(const AbstractFramebuffer* framebuffer,
                                      const ClearColor& color_value, float depth_value)
{
  if (g_state_tracker->GetFramebuffer() == framebuffer)
    return;

  g_state_tracker->SetFramebuffer(static_cast<const MetalFramebuffer*>(framebuffer));
  g_state_tracker->BeginRenderPass(mtlpp::LoadAction::Clear, color_value, depth_value);
}

void Renderer::SetViewport(float x, float y, float width, float height, float near_depth,
                           float far_depth)
{
  mtlpp::Viewport vp;
  vp.OriginX = x;
  vp.OriginY = y;
  vp.Width = width;
  vp.Height = height;
  vp.ZNear = near_depth;
  vp.ZFar = far_depth;
  g_state_tracker->SetViewport(vp);
}

void Renderer::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  mtlpp::ScissorRect sr;
  sr.X = rc.left;
  sr.Y = rc.top;
  sr.Width = rc.GetWidth();
  sr.Height = rc.GetHeight();

  // Must be at least 1x1.
  sr.Width = std::max(sr.Width, 1u);
  sr.Height = std::max(sr.Height, 1u);

  // Clamp to framebuffer bounds.
  const u32 target_width = static_cast<u32>(m_target_width);
  const u32 target_height = static_cast<u32>(m_target_height);
  if (sr.X >= target_width)
    sr.X = target_width - 1;
  if (sr.Y >= target_height)
    sr.Y = target_height - 1;
  if (sr.X + sr.Width > target_width)
    sr.Width = target_width - sr.X;
  if (sr.Y + sr.Height > target_height)
    sr.Height = target_height - sr.Y;

  g_state_tracker->SetScissor(sr);
}

void Renderer::ResetAPIState()
{
  ::Renderer::ResetAPIState();
}

void Renderer::RestoreAPIState()
{
  ::Renderer::RestoreAPIState();

  // Restore all GX state which may have been invalidated.
  g_state_tracker->SetFramebuffer(TextureCache::GetInstance()->GetEFBFramebuffer());

  // No need to update depth/blend state, as VertexManager tracks those.
  BPFunctions::SetViewport();
  BPFunctions::SetScissor();
}

void Renderer::SetInitialState()
{
  g_state_tracker->SetFramebuffer(TextureCache::GetInstance()->GetEFBFramebuffer());

  // Ensure all samplers are initialized. The debug layer whinges if we don't.
  for (u32 i = 0; i < MAX_TEXTURE_BINDINGS; i++)
    g_state_tracker->SetPixelSampler(i, g_metal_context->GetLinearSamplerState());

  BPFunctions::SetViewport();
  BPFunctions::SetScissor();
}

void Renderer::DrawUtilityPipeline(const void* uniforms, u32 uniforms_size, const void* vertices,
                                   u32 vertex_stride, u32 num_vertices)
{
  // Store uniforms
  if (uniforms_size > 0)
  {
    StreamBuffer* ubo = g_metal_context->GetUniformStreamBuffer();
    if (!ubo->ReserveMemory(uniforms_size, UNIFORM_BUFFER_ALIGNMENT))
    {
      SubmitCommandBuffer();
      if (!ubo->ReserveMemory(uniforms_size, UNIFORM_BUFFER_ALIGNMENT))
      {
        PanicAlert("Failed to reserve space for uniforms");
        return;
      }
    }

    g_state_tracker->SetVertexUniforms(ubo->GetBuffer(), ubo->GetCurrentOffset());
    g_state_tracker->SetPixelUniforms(0, ubo->GetBuffer(), ubo->GetCurrentOffset());
    std::memcpy(ubo->GetCurrentHostPointer(), uniforms, uniforms_size);
    ubo->CommitMemory(uniforms_size);
    ADDSTAT(stats.thisFrame.bytesUniformStreamed, static_cast<int>(uniforms_size));
  }

  // Store vertices
  if (vertices)
  {
    u32 batch_size =
        std::min(num_vertices, m_utility_vertex_buffer->GetCurrentSize() / vertex_stride);
    u32 num_batches = num_vertices / batch_size;
    const u8* vertex_ptr = reinterpret_cast<const u8*>(vertices);
    for (u32 i = 0; i < num_batches; i++)
    {
      u32 this_batch_vertices = std::min(num_vertices, batch_size);
      u32 vertices_size = this_batch_vertices * vertex_stride;
      if (!m_utility_vertex_buffer->ReserveMemory(vertices_size, vertex_stride))
      {
        SubmitCommandBuffer();
        if (!m_utility_vertex_buffer->ReserveMemory(vertices_size, vertex_stride))
        {
          PanicAlert("Failed to reserve space for vertices");
          return;
        }
      }

      g_state_tracker->SetVertexBuffer(m_utility_vertex_buffer->GetBuffer(),
                                       m_utility_vertex_buffer->GetCurrentOffset());
      std::memcpy(m_utility_vertex_buffer->GetCurrentHostPointer(), vertex_ptr, vertices_size);
      m_utility_vertex_buffer->CommitMemory(vertices_size);
      ADDSTAT(stats.thisFrame.bytesVertexStreamed, static_cast<int>(vertices_size));
      g_state_tracker->Draw(this_batch_vertices);
      vertex_ptr += vertices_size;
    }
  }
  else
  {
    g_state_tracker->Draw(num_vertices);
  }
}

void Renderer::DrawClearQuad(const MathUtil::Rectangle<int>& rc, bool clear_color, bool clear_alpha,
                             bool clear_depth, u32 color_val, float depth_val)
{
  VideoCommon::EFBClearPipelineConfig config(clear_color, clear_alpha, clear_depth);
  const AbstractPipeline* pipeline = g_shader_cache->GetEFBClearPipeline(config);
  if (!pipeline)
  {
    ERROR_LOG(VIDEO, "Failed to get pipeline for clear quad.");
    return;
  }

  g_state_tracker->SetViewportAndScissor(rc.left, rc.top, rc.GetWidth(), rc.GetHeight());

  auto vertices = Util::GetQuadVertices(depth_val, color_val);
  SetPipeline(pipeline);
  DrawUtilityPipeline(nullptr, 0, vertices.data(), sizeof(VideoCommon::UtilityVertex),
                      static_cast<u32>(vertices.size()));
  RestoreAPIState();
}

void Renderer::BlitScreen(const TargetRectangle& dst_rect, const TargetRectangle& src_rect,
                          const MetalTexture* src_tex)
{
  VideoCommon::BlitPipelineConfig config(g_state_tracker->GetFramebuffer()->GetColorFormat());
  const AbstractPipeline* pipeline = g_shader_cache->GetBlitPipeline(config);
  if (!pipeline)
  {
    ERROR_LOG(VIDEO, "Failed to get pipeline for blit quad.");
    return;
  }

  g_state_tracker->SetViewportAndScissor(dst_rect.left, dst_rect.top, dst_rect.GetWidth(),
                                         dst_rect.GetHeight());
  g_state_tracker->SetPixelTexture(0, src_tex);
  g_state_tracker->SetPixelSampler(0, g_metal_context->GetLinearSamplerState());

  auto vertices = Util::GetQuadVertices(src_rect, src_tex->GetWidth(), src_tex->GetHeight());
  SetPipeline(pipeline);
  DrawUtilityPipeline(nullptr, 0, vertices.data(), sizeof(VideoCommon::UtilityVertex),
                      static_cast<u32>(vertices.size()));
}

void Renderer::SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks, float gamma)
{
  // There are a few variables which can alter the final window draw rectangle, and some of them
  // are determined by guest state. Currently, the only way to catch these is to update every frame.
  UpdateDrawRectangle();

  // End any EFB render pass.
  g_state_tracker->EndRenderPass();

  // Draw to the screen if we have a backbuffer/window.
  if (m_backbuffer)
  {
    DrawScreen(static_cast<MetalTexture*>(texture), rc);
    SubmitCommandBuffer();
  }
  else
  {
    // Otherwise, just submit the command buffer.
    SubmitCommandBuffer();
  }

  // Determine what (if anything) has changed in the config.
  CheckForConfigChanges();

  // Begin rendering next frame.
  RestoreAPIState();

  // Clean up stale textures.
  TextureCache::GetInstance()->Cleanup(frameCount);
}

void Renderer::DrawScreen(const MetalTexture* xfb_texture, const EFBRectangle& xfb_region)
{
  if (!m_backbuffer->UpdateSurface())
    return;

  // Start render pass, and clear the framebuffer.
  g_state_tracker->SetFramebuffer(m_backbuffer.get());
  g_state_tracker->BeginRenderPass(mtlpp::LoadAction::Clear, {});

  // Draw the XFB.
  TargetRectangle target_rc = GetTargetRectangle();
  BlitScreen(target_rc, xfb_region, xfb_texture);

  // Draw the OSD.
  g_state_tracker->SetViewportAndScissor(0, 0, m_backbuffer->GetWidth(), m_backbuffer->GetHeight());
  DrawDebugText();
  OSD::DoCallbacks(OSD::CallbackType::OnFrame);
  OSD::DrawMessages();

  // Done rendering, present the image.
  g_state_tracker->EndRenderPass();
  m_backbuffer->Present();
}

void Renderer::CheckForConfigChanges()
{
  // Save the video config so we can compare against to determine which settings have changed.
  const u32 old_multisamples = g_ActiveConfig.iMultisamples;
  const int old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
  const bool old_force_filtering = g_ActiveConfig.bForceFiltering;

  // Copy g_Config to g_ActiveConfig.
  // NOTE: This can potentially race with the UI thread, however if it does, the changes will be
  // delayed until the next time CheckForConfigChanges is called.
  UpdateActiveConfig();

  // Determine which (if any) settings have changed.
  const bool multisamples_changed = old_multisamples != g_ActiveConfig.iMultisamples;
  const bool anisotropy_changed = old_anisotropy != g_ActiveConfig.iMaxAnisotropy;
  const bool force_texture_filtering_changed =
      old_force_filtering != g_ActiveConfig.bForceFiltering;

  // Update texture cache settings with any changed options.
  TextureCache::GetInstance()->OnConfigChanged(g_ActiveConfig);

  // Handle settings that can cause the EFB framebuffer to change.
  if (CalculateTargetSize() || multisamples_changed)
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    TextureCache::GetInstance()->RecreateEFBFramebuffer();
  }

  // MSAA samples changed, we need to recreate the EFB render pass.
  // If the stereoscopy mode changed, we need to recreate the buffers as well.
  // SSAA changed on/off, we have to recompile shaders.
  // Changing stereoscopy from off<->on also requires shaders to be recompiled.
  if (CheckForHostConfigChanges())
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    TextureCache::GetInstance()->RecreateEFBFramebuffer();
  }

  // If the anisotropy changes, we need to invalidate the sampler states.
  if (anisotropy_changed || force_texture_filtering_changed)
    ResetSamplerStates();
}

void Renderer::ResetSamplerStates()
{
  g_metal_context->ResetSamplerStates();
  for (u32 i = 0; i < MAX_TEXTURE_BINDINGS; i++)
    g_state_tracker->SetPixelSampler(i, g_metal_context->GetLinearSamplerState());
}

}  // namespace Metal
