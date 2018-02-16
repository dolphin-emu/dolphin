// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/TextureCache.h"
#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/Metal/Render.h"
#include "VideoBackends/Metal/StateTracker.h"
#include "VideoBackends/Metal/Util.h"
#include "VideoCommon/ShaderCache.h"

namespace Metal
{
bool TextureCache::Initialize()
{
  if (!CreateEFBFramebuffer())
  {
    PanicAlert("Failed to create EFB framebuffer.");
    return false;
  }

  if (!CreateCopyTextures())
  {
    PanicAlert("Failed to create EFB copy textures.");
    return false;
  }

  return true;
}

void TextureCache::CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                       const EFBRectangle& src_rect, bool scale_by_half,
                                       EFBCopyFormat dst_format, bool is_intensity)
{
  const MetalTexture* tex = static_cast<MetalTexture*>(entry->texture.get());
  _dbg_assert_(VIDEO, tex->HasFramebuffer());

  // Get pipeline for EFB copy shader.
  auto uid = TextureConversionShaderGen::GetShaderUid(dst_format, is_depth_copy, is_intensity,
                                                      scale_by_half);
  const AbstractPipeline* pipeline = g_shader_cache->GetEFBToTexturePipeline(uid);
  if (!pipeline)
  {
    WARN_LOG(VIDEO, "Skipping EFB copy to texture due to missing pipeline.");
    return;
  }

  // End rendering to the EFB.
  g_state_tracker->EndRenderPass();

  // TODO: Resolve multisampled framebuffer.
  TargetRectangle scaled_rect = g_renderer->ConvertEFBRectangle(src_rect);
  const MetalTexture* source_tex =
      is_depth_copy ? m_efb_depth_texture.get() : m_efb_color_texture.get();

  // Generate vertices with the texture coordinates set to the source rectangle.
  auto vertices =
      Util::GetQuadVertices(scaled_rect, source_tex->GetWidth(), source_tex->GetHeight());

  // Set up and issue the draw.
  g_renderer->SetAndDiscardFramebuffer(tex->GetFramebuffer());
  g_renderer->SetPipeline(pipeline);
  g_renderer->SetTexture(0, source_tex);
  g_renderer->SetSamplerState(0, scale_by_half ? RenderState::GetLinearSamplerState() :
                                                 RenderState::GetPointSamplerState());
  g_renderer->SetViewport(0, 0, tex->GetWidth(), tex->GetHeight(), 0.0f, 1.0f);
  g_renderer->SetScissorRect(MathUtil::Rectangle<int>(0, 0, tex->GetWidth(), tex->GetHeight()));
  g_renderer->DrawUtilityPipeline(nullptr, 0, vertices.data(), sizeof(VideoCommon::UtilityVertex),
                                  static_cast<u32>(vertices.size()));
  g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width,
                           u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                           const EFBRectangle& src_rect, bool scale_by_half)
{
  // Get pipeline for EFB copy shader.
  const AbstractPipeline* pipeline = g_shader_cache->GetEFBToRAMPipeline(params);
  if (!pipeline)
  {
    WARN_LOG(VIDEO, "Skipping EFB copy to RAM due to missing pipeline.");
    return;
  }

  // End rendering to the EFB.
  g_state_tracker->EndRenderPass();

  // TODO: Resolve multisampled framebuffer.
  // TargetRectangle scaled_rect = g_renderer->ConvertEFBRectangle(src_rect);
  // scaled_rect.ClampUL(0, 0, m_efb_color_texture->GetWidth(), m_efb_color_texture->GetHeight());
  const MetalTexture* source_tex =
      params.depth ? m_efb_depth_texture.get() : m_efb_color_texture.get();

  // Build uniforms for shader.
  struct EFBEncodeParams
  {
    std::array<s32, 4> position_uniform;
    float y_scale;
  };
  EFBEncodeParams uniforms;
  uniforms.position_uniform[0] = src_rect.left;
  uniforms.position_uniform[1] = src_rect.top;
  uniforms.position_uniform[2] = static_cast<s32>(native_width);
  uniforms.position_uniform[3] = scale_by_half ? 2 : 1;
  uniforms.y_scale = params.y_scale;

  // Begin draw setup.
  g_renderer->SetAndDiscardFramebuffer(m_copy_render_texture->GetFramebuffer());
  g_renderer->SetPipeline(pipeline);
  g_renderer->SetTexture(0, source_tex);

  // We also linear filtering for both box filtering and downsampling higher resolutions to 1x
  // TODO: This only produces perfect downsampling for 2x IR, other resolutions will need more
  //       complex down filtering to average all pixels and produce the correct result.
  bool linear_filter =
      (scale_by_half && !params.depth) || g_renderer->GetEFBScale() != 1 || params.y_scale > 1.0f;
  g_renderer->SetSamplerState(0, linear_filter ? RenderState::GetLinearSamplerState() :
                                                 RenderState::GetPointSamplerState());

  u32 render_width = bytes_per_row / sizeof(u32);
  u32 render_height = num_blocks_y;
  g_renderer->SetViewport(0, 0, render_width, render_height, 0.0f, 1.0f);
  g_renderer->SetScissorRect(MathUtil::Rectangle<int>(0, 0, render_width, render_height));

  // Dispatch the draw and end the render pass.
  g_renderer->DrawUtilityPipeline(&uniforms, sizeof(uniforms), nullptr, 0, 4);
  g_state_tracker->EndRenderPass();

  // Copy to the staging texture, and then to guest RAM.
  MathUtil::Rectangle<int> copy_rect(0, 0, render_width, render_height);
  m_copy_readback_texture->CopyFromTexture(m_copy_render_texture.get(), copy_rect, 0, 0, copy_rect);
  m_copy_readback_texture->ReadTexels(copy_rect, dst, memory_stride);
  g_renderer->RestoreAPIState();
}

bool TextureCache::CreateEFBFramebuffer()
{
  const u32 width = g_renderer->GetTargetWidth();
  const u32 height = g_renderer->GetTargetHeight();
  const u32 layers = 1;
  const u32 samples = 1;

  const TextureConfig color_cfg(width, height, 1, layers, samples, EFB_COLOR_TEXTURE_FORMAT, true);
  const TextureConfig depth_cfg(width, height, 1, layers, samples, EFB_DEPTH_TEXTURE_FORMAT, true);
  INFO_LOG(VIDEO, "Creating EFB framebuffer (%ux%ux%u, %u samples)", width, height, layers,
           samples);

  m_efb_color_texture = MetalTexture::Create(color_cfg);
  m_efb_depth_texture = MetalTexture::Create(depth_cfg);
  if (!m_efb_color_texture || !m_efb_depth_texture)
    return false;

  m_efb_framebuffer = MetalFramebuffer::CreateForAbstractTexture(m_efb_color_texture.get(),
                                                                 m_efb_depth_texture.get());
  if (!m_efb_framebuffer)
    return false;

  // TODO: Should we clear these textures first?
  return true;
}

void TextureCache::DestroyEFBFramebuffer()
{
  m_efb_framebuffer.reset();
  m_efb_depth_texture.reset();
  m_efb_color_texture.reset();
}

void TextureCache::RecreateEFBFramebuffer()
{
  DestroyEFBFramebuffer();
  if (!CreateEFBFramebuffer())
    PanicAlert("Failed to recreate EFB framebuffer");
}

bool TextureCache::CreateCopyTextures()
{
  TextureConfig config(ENCODING_TEXTURE_WIDTH, ENCODING_TEXTURE_HEIGHT, 1, 1, 1,
                       ENCODING_TEXTURE_FORMAT, true);

  m_copy_render_texture = MetalTexture::Create(config);
  m_copy_readback_texture = MetalStagingTexture::Create(StagingTextureType::Readback, config);
  return m_copy_render_texture && m_copy_readback_texture;
}

}  // namespace Metal
