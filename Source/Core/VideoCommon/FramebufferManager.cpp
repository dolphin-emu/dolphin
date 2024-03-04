// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FramebufferManager.h"

#include <fmt/format.h>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/System.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

// Maximum number of pixels poked in one batch * 6
constexpr size_t MAX_POKE_VERTICES = 32768;

std::unique_ptr<FramebufferManager> g_framebuffer_manager;

FramebufferManager::FramebufferManager() : m_prev_efb_format(PixelFormat::INVALID_FMT)
{
}

FramebufferManager::~FramebufferManager()
{
  DestroyClearPipelines();
  DestroyPokePipelines();
  DestroyConversionPipelines();
  DestroyReadbackPipelines();
  DestroyReadbackFramebuffer();
  DestroyEFBFramebuffer();
}

bool FramebufferManager::Initialize()
{
  if (!CreateEFBFramebuffer())
  {
    PanicAlertFmt("Failed to create EFB framebuffer");
    return false;
  }

  m_efb_cache_tile_size = static_cast<u32>(std::max(g_ActiveConfig.iEFBAccessTileSize, 0));
  if (!CreateReadbackFramebuffer())
  {
    PanicAlertFmt("Failed to create EFB readback framebuffer");
    return false;
  }

  if (!CompileReadbackPipelines())
  {
    PanicAlertFmt("Failed to compile EFB readback pipelines");
    return false;
  }

  if (!CompileConversionPipelines())
  {
    PanicAlertFmt("Failed to compile EFB conversion pipelines");
    return false;
  }

  if (!CompileClearPipelines())
  {
    PanicAlertFmt("Failed to compile EFB clear pipelines");
    return false;
  }

  if (!CompilePokePipelines())
  {
    PanicAlertFmt("Failed to compile EFB poke pipelines");
    return false;
  }

  m_end_of_frame_event =
      AfterFrameEvent::Register([this](Core::System&) { EndOfFrame(); }, "FramebufferManager");

  return true;
}

void FramebufferManager::RecreateEFBFramebuffer()
{
  FlushEFBPokes();
  InvalidatePeekCache(true);

  DestroyReadbackFramebuffer();
  DestroyEFBFramebuffer();
  if (!CreateEFBFramebuffer() || !CreateReadbackFramebuffer())
    PanicAlertFmt("Failed to recreate EFB framebuffer");
}

void FramebufferManager::RecompileShaders()
{
  DestroyPokePipelines();
  DestroyClearPipelines();
  DestroyConversionPipelines();
  DestroyReadbackPipelines();
  if (!CompileReadbackPipelines() || !CompileConversionPipelines() || !CompileClearPipelines() ||
      !CompilePokePipelines())
  {
    PanicAlertFmt("Failed to recompile EFB pipelines");
  }
}

AbstractTextureFormat FramebufferManager::GetEFBColorFormat()
{
  // The EFB can be set to different pixel formats by the game through the
  // BPMEM_ZCOMPARE register (which should probably have a different name).
  // They are:
  // - 24-bit RGB (8-bit components) with 24-bit Z
  // - 24-bit RGBA (6-bit components) with 24-bit Z
  // - Multisampled 16-bit RGB (5-6-5 format) with 16-bit Z
  // We only use one EFB format here: 32-bit ARGB with 32-bit Z.
  // Multisampling depends on user settings.
  // The distinction becomes important for certain operations, i.e. the
  // alpha channel should be ignored if the EFB does not have one.
  return AbstractTextureFormat::RGBA8;
}

AbstractTextureFormat FramebufferManager::GetEFBDepthFormat()
{
  // 32-bit depth clears are broken in the Adreno Vulkan driver, and have no effect.
  // To work around this, we use a D24_S8 buffer instead, which results in a loss of accuracy.
  // We still resolve this to a R32F texture, as there is no 24-bit format.
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_D32F_CLEAR))
    return AbstractTextureFormat::D24_S8;
  else
    return AbstractTextureFormat::D32F;
}

AbstractTextureFormat FramebufferManager::GetEFBDepthCopyFormat()
{
  return AbstractTextureFormat::R32F;
}

static u32 CalculateEFBLayers()
{
  return (g_ActiveConfig.stereo_mode != StereoMode::Off) ? 2 : 1;
}

TextureConfig FramebufferManager::GetEFBColorTextureConfig(u32 width, u32 height)
{
  return TextureConfig(width, height, 1, CalculateEFBLayers(), g_ActiveConfig.iMultisamples,
                       GetEFBColorFormat(), AbstractTextureFlag_RenderTarget,
                       AbstractTextureType::Texture_2DArray);
}

TextureConfig FramebufferManager::GetEFBDepthTextureConfig(u32 width, u32 height)
{
  return TextureConfig(width, height, 1, CalculateEFBLayers(), g_ActiveConfig.iMultisamples,
                       GetEFBDepthFormat(), AbstractTextureFlag_RenderTarget,
                       AbstractTextureType::Texture_2DArray);
}

FramebufferState FramebufferManager::GetEFBFramebufferState() const
{
  FramebufferState ret = {};
  ret.color_texture_format = m_efb_color_texture->GetFormat();
  ret.depth_texture_format = m_efb_depth_texture->GetFormat();
  ret.per_sample_shading = IsEFBMultisampled() && g_ActiveConfig.bSSAA;
  ret.samples = m_efb_color_texture->GetSamples();
  return ret;
}

MathUtil::Rectangle<int>
FramebufferManager::ConvertEFBRectangle(const MathUtil::Rectangle<int>& rc) const
{
  MathUtil::Rectangle<int> result;
  result.left = EFBToScaledX(rc.left);
  result.top = EFBToScaledY(rc.top);
  result.right = EFBToScaledX(rc.right);
  result.bottom = EFBToScaledY(rc.bottom);
  return result;
}

unsigned int FramebufferManager::GetEFBScale() const
{
  return m_efb_scale;
}

int FramebufferManager::EFBToScaledX(int x) const
{
  return x * static_cast<int>(m_efb_scale);
}

int FramebufferManager::EFBToScaledY(int y) const
{
  return y * static_cast<int>(m_efb_scale);
}

float FramebufferManager::EFBToScaledXf(float x) const
{
  return x * ((float)GetEFBWidth() / (float)EFB_WIDTH);
}

float FramebufferManager::EFBToScaledYf(float y) const
{
  return y * ((float)GetEFBHeight() / (float)EFB_HEIGHT);
}

std::tuple<u32, u32> FramebufferManager::CalculateTargetSize()
{
  if (g_ActiveConfig.iEFBScale == EFB_SCALE_AUTO_INTEGRAL)
    m_efb_scale = g_presenter->AutoIntegralScale();
  else
    m_efb_scale = g_ActiveConfig.iEFBScale;

  const u32 max_size = g_ActiveConfig.backend_info.MaxTextureSize;
  if (max_size < EFB_WIDTH * m_efb_scale)
    m_efb_scale = max_size / EFB_WIDTH;

  u32 new_efb_width = std::max(EFB_WIDTH * static_cast<int>(m_efb_scale), 1u);
  u32 new_efb_height = std::max(EFB_HEIGHT * static_cast<int>(m_efb_scale), 1u);

  return std::make_tuple(new_efb_width, new_efb_height);
}

bool FramebufferManager::CreateEFBFramebuffer()
{
  auto [width, height] = CalculateTargetSize();

  const TextureConfig efb_color_texture_config = GetEFBColorTextureConfig(width, height);
  const TextureConfig efb_depth_texture_config = GetEFBDepthTextureConfig(width, height);

  // We need a second texture to swap with for changing pixel formats
  m_efb_color_texture = g_gfx->CreateTexture(efb_color_texture_config, "EFB color texture");
  m_efb_depth_texture = g_gfx->CreateTexture(efb_depth_texture_config, "EFB depth texture");
  m_efb_convert_color_texture =
      g_gfx->CreateTexture(efb_color_texture_config, "EFB convert color texture");
  if (!m_efb_color_texture || !m_efb_depth_texture || !m_efb_convert_color_texture)
    return false;

  m_efb_framebuffer =
      g_gfx->CreateFramebuffer(m_efb_color_texture.get(), m_efb_depth_texture.get());
  m_efb_convert_framebuffer =
      g_gfx->CreateFramebuffer(m_efb_convert_color_texture.get(), m_efb_depth_texture.get());
  if (!m_efb_framebuffer || !m_efb_convert_framebuffer)
    return false;

  // Create resolved textures if MSAA is on
  if (g_ActiveConfig.MultisamplingEnabled())
  {
    u32 flags = 0;
    if (!g_ActiveConfig.backend_info.bSupportsPartialMultisampleResolve)
      flags |= AbstractTextureFlag_RenderTarget;
    m_efb_resolve_color_texture = g_gfx->CreateTexture(
        TextureConfig(efb_color_texture_config.width, efb_color_texture_config.height, 1,
                      efb_color_texture_config.layers, 1, efb_color_texture_config.format, flags,
                      AbstractTextureType::Texture_2DArray),
        "EFB color resolve texture");
    if (!m_efb_resolve_color_texture)
      return false;

    if (!g_ActiveConfig.backend_info.bSupportsPartialMultisampleResolve)
    {
      m_efb_color_resolve_framebuffer =
          g_gfx->CreateFramebuffer(m_efb_resolve_color_texture.get(), nullptr);
      if (!m_efb_color_resolve_framebuffer)
        return false;
    }
  }

  // We also need one to convert the D24S8 to R32F if that is being used (Adreno).
  if (g_ActiveConfig.MultisamplingEnabled() || GetEFBDepthFormat() != AbstractTextureFormat::R32F)
  {
    m_efb_depth_resolve_texture = g_gfx->CreateTexture(
        TextureConfig(efb_depth_texture_config.width, efb_depth_texture_config.height, 1,
                      efb_depth_texture_config.layers, 1, GetEFBDepthCopyFormat(),
                      AbstractTextureFlag_RenderTarget, AbstractTextureType::Texture_2DArray),
        "EFB depth resolve texture");
    if (!m_efb_depth_resolve_texture)
      return false;

    m_efb_depth_resolve_framebuffer =
        g_gfx->CreateFramebuffer(m_efb_depth_resolve_texture.get(), nullptr);
    if (!m_efb_depth_resolve_framebuffer)
      return false;
  }

  // Clear the renderable textures out.
  g_gfx->SetAndClearFramebuffer(m_efb_framebuffer.get(), {{0.0f, 0.0f, 0.0f, 0.0f}},
                                g_ActiveConfig.backend_info.bSupportsReversedDepthRange ? 1.0f :
                                                                                          0.0f);
  return true;
}

void FramebufferManager::DestroyEFBFramebuffer()
{
  m_efb_framebuffer.reset();
  m_efb_convert_framebuffer.reset();
  m_efb_color_texture.reset();
  m_efb_convert_color_texture.reset();
  m_efb_depth_texture.reset();
  m_efb_resolve_color_texture.reset();
  m_efb_depth_resolve_framebuffer.reset();
  m_efb_depth_resolve_texture.reset();
}

void FramebufferManager::BindEFBFramebuffer()
{
  g_gfx->SetFramebuffer(m_efb_framebuffer.get());
}

AbstractTexture* FramebufferManager::ResolveEFBColorTexture(const MathUtil::Rectangle<int>& region)
{
  // Return the normal EFB texture if multisampling is off.
  if (!IsEFBMultisampled())
    return m_efb_color_texture.get();

  // It's not valid to resolve an out-of-range rectangle.
  MathUtil::Rectangle<int> clamped_region = region;
  clamped_region.ClampUL(0, 0, GetEFBWidth(), GetEFBHeight());

  // Resolve to our already-created texture.
  if (g_ActiveConfig.backend_info.bSupportsPartialMultisampleResolve)
  {
    for (u32 layer = 0; layer < GetEFBLayers(); layer++)
    {
      m_efb_resolve_color_texture->ResolveFromTexture(m_efb_color_texture.get(), clamped_region,
                                                      layer, 0);
    }
  }
  else
  {
    m_efb_color_texture->FinishedRendering();
    g_gfx->BeginUtilityDrawing();
    g_gfx->SetAndDiscardFramebuffer(m_efb_color_resolve_framebuffer.get());
    g_gfx->SetPipeline(m_efb_color_resolve_pipeline.get());
    g_gfx->SetTexture(0, m_efb_color_texture.get());
    g_gfx->SetSamplerState(0, RenderState::GetPointSamplerState());
    g_gfx->SetViewportAndScissor(clamped_region);
    g_gfx->Draw(0, 3);
    m_efb_resolve_color_texture->FinishedRendering();
    g_gfx->EndUtilityDrawing();
  }
  m_efb_resolve_color_texture->FinishedRendering();
  return m_efb_resolve_color_texture.get();
}

AbstractTexture* FramebufferManager::ResolveEFBDepthTexture(const MathUtil::Rectangle<int>& region,
                                                            bool force_r32f)
{
  if (!IsEFBMultisampled() &&
      (!force_r32f || m_efb_depth_texture->GetFormat() == AbstractTextureFormat::D32F))
  {
    return m_efb_depth_texture.get();
  }

  // It's not valid to resolve an out-of-range rectangle.
  MathUtil::Rectangle<int> clamped_region = region;
  clamped_region.ClampUL(0, 0, GetEFBWidth(), GetEFBHeight());

  m_efb_depth_texture->FinishedRendering();
  g_gfx->BeginUtilityDrawing();
  g_gfx->SetAndDiscardFramebuffer(m_efb_depth_resolve_framebuffer.get());
  g_gfx->SetPipeline(IsEFBMultisampled() ? m_efb_depth_resolve_pipeline.get() :
                                           m_efb_depth_cache.copy_pipeline.get());
  g_gfx->SetTexture(0, m_efb_depth_texture.get());
  g_gfx->SetSamplerState(0, RenderState::GetPointSamplerState());
  g_gfx->SetViewportAndScissor(clamped_region);
  g_gfx->Draw(0, 3);
  m_efb_depth_resolve_texture->FinishedRendering();
  g_gfx->EndUtilityDrawing();

  return m_efb_depth_resolve_texture.get();
}

bool FramebufferManager::ReinterpretPixelData(EFBReinterpretType convtype)
{
  if (!m_format_conversion_pipelines[static_cast<u32>(convtype)])
    return false;

  // Draw to the secondary framebuffer.
  // We don't discard here because discarding the framebuffer also throws away the depth
  // buffer, which we want to preserve. If we find this to be hindering performance in the
  // future (e.g. on mobile/tilers), it may be worth discarding only the color buffer.
  m_efb_color_texture->FinishedRendering();
  g_gfx->BeginUtilityDrawing();
  g_gfx->SetFramebuffer(m_efb_convert_framebuffer.get());
  g_gfx->SetViewportAndScissor(m_efb_framebuffer->GetRect());
  g_gfx->SetPipeline(m_format_conversion_pipelines[static_cast<u32>(convtype)].get());
  g_gfx->SetTexture(0, m_efb_color_texture.get());
  g_gfx->Draw(0, 3);

  // And swap the framebuffers around, so we do new drawing to the converted framebuffer.
  std::swap(m_efb_color_texture, m_efb_convert_color_texture);
  std::swap(m_efb_framebuffer, m_efb_convert_framebuffer);
  g_gfx->EndUtilityDrawing();
  InvalidatePeekCache(true);
  return true;
}

bool FramebufferManager::CompileConversionPipelines()
{
  for (u32 i = 0; i < NUM_EFB_REINTERPRET_TYPES; i++)
  {
    EFBReinterpretType convtype = static_cast<EFBReinterpretType>(i);
    std::unique_ptr<AbstractShader> pixel_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Pixel,
        FramebufferShaderGen::GenerateFormatConversionShader(convtype, GetEFBSamples()),
        fmt::format("Framebuffer conversion pixel shader {}", convtype));
    if (!pixel_shader)
      return false;

    AbstractPipelineConfig config = {};
    config.vertex_shader = g_shader_cache->GetScreenQuadVertexShader();
    config.geometry_shader = IsEFBStereo() ? g_shader_cache->GetTexcoordGeometryShader() : nullptr;
    config.pixel_shader = pixel_shader.get();
    config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
    config.depth_state = RenderState::GetNoDepthTestingDepthState();
    config.blending_state = RenderState::GetNoBlendingBlendState();
    config.framebuffer_state = GetEFBFramebufferState();
    config.usage = AbstractPipelineUsage::Utility;
    m_format_conversion_pipelines[i] = g_gfx->CreatePipeline(config);
    if (!m_format_conversion_pipelines[i])
      return false;
  }

  return true;
}

void FramebufferManager::DestroyConversionPipelines()
{
  for (auto& pipeline : m_format_conversion_pipelines)
    pipeline.reset();
}

bool FramebufferManager::IsUsingTiledEFBCache() const
{
  return m_efb_cache_tile_size > 0;
}

bool FramebufferManager::IsEFBCacheTilePresent(bool depth, u32 x, u32 y, u32* tile_index) const
{
  const EFBCacheData& data = depth ? m_efb_depth_cache : m_efb_color_cache;
  if (!IsUsingTiledEFBCache())
  {
    *tile_index = 0;
  }
  else
  {
    const u32 tile_x = x / m_efb_cache_tile_size;
    const u32 tile_y = y / m_efb_cache_tile_size;
    *tile_index = (tile_y * m_efb_cache_tile_row_stride) + tile_x;
  }
  return data.tiles[*tile_index].present;
}

MathUtil::Rectangle<int> FramebufferManager::GetEFBCacheTileRect(u32 tile_index) const
{
  if (!IsUsingTiledEFBCache())
    return MathUtil::Rectangle<int>(0, 0, EFB_WIDTH, EFB_HEIGHT);

  const u32 tile_y = tile_index / m_efb_cache_tile_row_stride;
  const u32 tile_x = tile_index % m_efb_cache_tile_row_stride;
  const u32 start_y = tile_y * m_efb_cache_tile_size;
  const u32 start_x = tile_x * m_efb_cache_tile_size;
  return MathUtil::Rectangle<int>(
      start_x, start_y, std::min(start_x + m_efb_cache_tile_size, static_cast<u32>(EFB_WIDTH)),
      std::min(start_y + m_efb_cache_tile_size, static_cast<u32>(EFB_HEIGHT)));
}

u32 FramebufferManager::PeekEFBColor(u32 x, u32 y)
{
  // The y coordinate here assumes upper-left origin, but the readback texture is lower-left in GL.
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
    y = EFB_HEIGHT - 1 - y;

  u32 tile_index;
  if (!IsEFBCacheTilePresent(false, x, y, &tile_index))
    PopulateEFBCache(false, tile_index);

  m_efb_color_cache.tiles[tile_index].frame_access_mask |= 1;

  if (m_efb_color_cache.needs_flush)
  {
    m_efb_color_cache.readback_texture->Flush();
    m_efb_color_cache.needs_flush = false;
  }

  u32 value;
  m_efb_color_cache.readback_texture->ReadTexel(x, y, &value);
  return value;
}

float FramebufferManager::PeekEFBDepth(u32 x, u32 y)
{
  // The y coordinate here assumes upper-left origin, but the readback texture is lower-left in GL.
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
    y = EFB_HEIGHT - 1 - y;

  u32 tile_index;
  if (!IsEFBCacheTilePresent(true, x, y, &tile_index))
    PopulateEFBCache(true, tile_index);

  m_efb_depth_cache.tiles[tile_index].frame_access_mask |= 1;

  if (m_efb_depth_cache.needs_flush)
  {
    m_efb_depth_cache.readback_texture->Flush();
    m_efb_depth_cache.needs_flush = false;
  }

  float value;
  m_efb_depth_cache.readback_texture->ReadTexel(x, y, &value);
  return value;
}

void FramebufferManager::SetEFBCacheTileSize(u32 size)
{
  if (m_efb_cache_tile_size == size)
    return;

  InvalidatePeekCache(true);
  m_efb_cache_tile_size = size;
  DestroyReadbackFramebuffer();
  if (!CreateReadbackFramebuffer())
    PanicAlertFmt("Failed to create EFB readback framebuffers");
}

void FramebufferManager::RefreshPeekCache()
{
  if (!m_efb_color_cache.needs_refresh && !m_efb_depth_cache.needs_refresh)
  {
    // The cache has already been refreshed.
    return;
  }

  bool flush_command_buffer = false;
  for (u32 i = 0; i < m_efb_color_cache.tiles.size(); i++)
  {
    if (m_efb_color_cache.tiles[i].frame_access_mask != 0 && !m_efb_color_cache.tiles[i].present)
    {
      PopulateEFBCache(false, i, true);
      flush_command_buffer = true;
    }
    if (m_efb_depth_cache.tiles[i].frame_access_mask != 0 && !m_efb_depth_cache.tiles[i].present)
    {
      PopulateEFBCache(true, i, true);
      flush_command_buffer = true;
    }
  }

  m_efb_depth_cache.needs_refresh = false;
  m_efb_color_cache.needs_refresh = false;

  if (flush_command_buffer)
  {
    g_gfx->Flush();
  }
}

void FramebufferManager::InvalidatePeekCache(bool forced)
{
  if (forced || m_efb_color_cache.out_of_date)
  {
    if (m_efb_color_cache.has_active_tiles)
    {
      for (u32 i = 0; i < m_efb_color_cache.tiles.size(); i++)
      {
        m_efb_color_cache.tiles[i].present = false;
      }

      m_efb_color_cache.needs_refresh = true;
    }

    m_efb_color_cache.has_active_tiles = false;
    m_efb_color_cache.out_of_date = false;
  }
  if (forced || m_efb_depth_cache.out_of_date)
  {
    if (m_efb_depth_cache.has_active_tiles)
    {
      for (u32 i = 0; i < m_efb_depth_cache.tiles.size(); i++)
      {
        m_efb_depth_cache.tiles[i].present = false;
      }

      m_efb_depth_cache.needs_refresh = true;
    }

    m_efb_depth_cache.has_active_tiles = false;
    m_efb_depth_cache.out_of_date = false;
  }
}

void FramebufferManager::FlagPeekCacheAsOutOfDate()
{
  if (m_efb_color_cache.has_active_tiles)
    m_efb_color_cache.out_of_date = true;
  if (m_efb_depth_cache.has_active_tiles)
    m_efb_depth_cache.out_of_date = true;

  if (!g_ActiveConfig.bEFBAccessDeferInvalidation)
    InvalidatePeekCache();
}

void FramebufferManager::EndOfFrame()
{
  for (u32 i = 0; i < m_efb_color_cache.tiles.size(); i++)
  {
    m_efb_color_cache.tiles[i].frame_access_mask <<= 1;
    m_efb_depth_cache.tiles[i].frame_access_mask <<= 1;
  }
}

bool FramebufferManager::CompileReadbackPipelines()
{
  AbstractPipelineConfig config = {};
  config.vertex_shader = g_shader_cache->GetTextureCopyVertexShader();
  config.geometry_shader = IsEFBStereo() ? g_shader_cache->GetTexcoordGeometryShader() : nullptr;
  config.pixel_shader = g_shader_cache->GetTextureCopyPixelShader();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(GetEFBColorFormat());
  config.usage = AbstractPipelineUsage::Utility;
  m_efb_color_cache.copy_pipeline = g_gfx->CreatePipeline(config);
  if (!m_efb_color_cache.copy_pipeline)
    return false;

  // same for depth, except different format
  config.framebuffer_state.color_texture_format = GetEFBDepthCopyFormat();
  m_efb_depth_cache.copy_pipeline = g_gfx->CreatePipeline(config);
  if (!m_efb_depth_cache.copy_pipeline)
    return false;

  if (IsEFBMultisampled())
  {
    auto depth_resolve_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Pixel, FramebufferShaderGen::GenerateResolveDepthPixelShader(GetEFBSamples()),
        "Depth resolve pixel shader");
    if (!depth_resolve_shader)
      return false;

    config.pixel_shader = depth_resolve_shader.get();
    m_efb_depth_resolve_pipeline = g_gfx->CreatePipeline(config);
    if (!m_efb_depth_resolve_pipeline)
      return false;

    if (!g_ActiveConfig.backend_info.bSupportsPartialMultisampleResolve)
    {
      config.framebuffer_state.color_texture_format = GetEFBColorFormat();
      auto color_resolve_shader = g_gfx->CreateShaderFromSource(
          ShaderStage::Pixel,
          FramebufferShaderGen::GenerateResolveColorPixelShader(GetEFBSamples()),
          "Color resolve pixel shader");
      if (!color_resolve_shader)
        return false;

      config.pixel_shader = color_resolve_shader.get();
      m_efb_color_resolve_pipeline = g_gfx->CreatePipeline(config);
      if (!m_efb_color_resolve_pipeline)
        return false;
    }
  }

  // EFB restore pipeline
  auto restore_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateEFBRestorePixelShader(),
      "EFB restore pixel shader");
  if (!restore_shader)
    return false;

  config.depth_state = RenderState::GetAlwaysWriteDepthState();
  config.framebuffer_state = GetEFBFramebufferState();
  config.framebuffer_state.per_sample_shading = false;
  config.vertex_shader = g_shader_cache->GetScreenQuadVertexShader();
  config.pixel_shader = restore_shader.get();
  m_efb_restore_pipeline = g_gfx->CreatePipeline(config);
  if (!m_efb_restore_pipeline)
    return false;

  return true;
}

void FramebufferManager::DestroyReadbackPipelines()
{
  m_efb_depth_resolve_pipeline.reset();
  m_efb_depth_cache.copy_pipeline.reset();
  m_efb_color_cache.copy_pipeline.reset();
}

bool FramebufferManager::CreateReadbackFramebuffer()
{
  if (GetEFBScale() != 1)
  {
    const TextureConfig color_config(IsUsingTiledEFBCache() ? m_efb_cache_tile_size : EFB_WIDTH,
                                     IsUsingTiledEFBCache() ? m_efb_cache_tile_size : EFB_HEIGHT, 1,
                                     1, 1, GetEFBColorFormat(), AbstractTextureFlag_RenderTarget,
                                     AbstractTextureType::Texture_2DArray);
    m_efb_color_cache.texture = g_gfx->CreateTexture(color_config, "EFB color cache");
    if (!m_efb_color_cache.texture)
      return false;

    m_efb_color_cache.framebuffer =
        g_gfx->CreateFramebuffer(m_efb_color_cache.texture.get(), nullptr);
    if (!m_efb_color_cache.framebuffer)
      return false;
  }

  // Since we can't partially copy from a depth buffer directly to the staging texture in D3D, we
  // use an intermediate buffer to avoid copying the whole texture.
  if (!g_ActiveConfig.backend_info.bSupportsDepthReadback ||
      (IsUsingTiledEFBCache() && !g_ActiveConfig.backend_info.bSupportsPartialDepthCopies) ||
      !AbstractTexture::IsCompatibleDepthAndColorFormats(m_efb_depth_texture->GetFormat(),
                                                         GetEFBDepthCopyFormat()) ||
      GetEFBScale() != 1)
  {
    const TextureConfig depth_config(IsUsingTiledEFBCache() ? m_efb_cache_tile_size : EFB_WIDTH,
                                     IsUsingTiledEFBCache() ? m_efb_cache_tile_size : EFB_HEIGHT, 1,
                                     1, 1, GetEFBDepthCopyFormat(),
                                     AbstractTextureFlag_RenderTarget,
                                     AbstractTextureType::Texture_2DArray);
    m_efb_depth_cache.texture = g_gfx->CreateTexture(depth_config, "EFB depth cache");
    if (!m_efb_depth_cache.texture)
      return false;

    m_efb_depth_cache.framebuffer =
        g_gfx->CreateFramebuffer(m_efb_depth_cache.texture.get(), nullptr);
    if (!m_efb_depth_cache.framebuffer)
      return false;
  }

  // Staging texture use the full EFB dimensions, as this is the buffer for the whole cache.
  m_efb_color_cache.readback_texture =
      g_gfx->CreateStagingTexture(StagingTextureType::Mutable,
                                  TextureConfig(EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, GetEFBColorFormat(),
                                                0, AbstractTextureType::Texture_2DArray));
  m_efb_depth_cache.readback_texture = g_gfx->CreateStagingTexture(
      StagingTextureType::Mutable,
      TextureConfig(EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, GetEFBDepthCopyFormat(), 0,
                    AbstractTextureType::Texture_2DArray));
  if (!m_efb_color_cache.readback_texture || !m_efb_depth_cache.readback_texture)
    return false;

  u32 total_tiles = 1;
  if (IsUsingTiledEFBCache())
  {
    const u32 tiles_wide = ((EFB_WIDTH + (m_efb_cache_tile_size - 1)) / m_efb_cache_tile_size);
    const u32 tiles_high = ((EFB_HEIGHT + (m_efb_cache_tile_size - 1)) / m_efb_cache_tile_size);
    total_tiles = tiles_wide * tiles_high;
    m_efb_cache_tile_row_stride = tiles_wide;
  }
  else
  {
    m_efb_cache_tile_row_stride = 1;
  }

  m_efb_color_cache.tiles.resize(total_tiles);
  std::fill(m_efb_color_cache.tiles.begin(), m_efb_color_cache.tiles.end(), EFBCacheTile{false, 0});
  m_efb_depth_cache.tiles.resize(total_tiles);
  std::fill(m_efb_depth_cache.tiles.begin(), m_efb_depth_cache.tiles.end(), EFBCacheTile{false, 0});

  return true;
}

void FramebufferManager::DestroyReadbackFramebuffer()
{
  auto DestroyCache = [](EFBCacheData& data) {
    data.readback_texture.reset();
    data.framebuffer.reset();
    data.texture.reset();
    data.needs_refresh = false;
    data.has_active_tiles = false;
  };
  DestroyCache(m_efb_color_cache);
  DestroyCache(m_efb_depth_cache);
}

void FramebufferManager::PopulateEFBCache(bool depth, u32 tile_index, bool async)
{
  FlushEFBPokes();
  g_vertex_manager->OnCPUEFBAccess();

  // Force the path through the intermediate texture, as we can't do an image copy from a depth
  // buffer directly to a staging texture (must be the whole resource).
  const bool force_intermediate_copy =
      depth &&
      (!g_ActiveConfig.backend_info.bSupportsDepthReadback ||
       (!g_ActiveConfig.backend_info.bSupportsPartialDepthCopies && IsUsingTiledEFBCache()) ||
       !AbstractTexture::IsCompatibleDepthAndColorFormats(m_efb_depth_texture->GetFormat(),
                                                          GetEFBDepthCopyFormat()));

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  EFBCacheData& data = depth ? m_efb_depth_cache : m_efb_color_cache;
  const MathUtil::Rectangle<int> rect = GetEFBCacheTileRect(tile_index);
  const MathUtil::Rectangle<int> native_rect = ConvertEFBRectangle(rect);
  AbstractTexture* src_texture =
      depth ? ResolveEFBDepthTexture(native_rect) : ResolveEFBColorTexture(native_rect);
  if (GetEFBScale() != 1 || force_intermediate_copy)
  {
    // Downsample from internal resolution to 1x.
    // TODO: This won't produce correct results at IRs above 2x. More samples are required.
    // This is the same issue as with EFB copies.
    src_texture->FinishedRendering();
    g_gfx->BeginUtilityDrawing();

    const float rcp_src_width = 1.0f / m_efb_framebuffer->GetWidth();
    const float rcp_src_height = 1.0f / m_efb_framebuffer->GetHeight();
    const std::array<float, 4> uniforms = {
        {native_rect.left * rcp_src_width, native_rect.top * rcp_src_height,
         native_rect.GetWidth() * rcp_src_width, native_rect.GetHeight() * rcp_src_height}};
    g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

    // Viewport will not be TILE_SIZExTILE_SIZE for the last row of tiles, assuming a tile size of
    // 64, because 528 is not evenly divisible by 64.
    g_gfx->SetAndDiscardFramebuffer(data.framebuffer.get());
    g_gfx->SetViewportAndScissor(MathUtil::Rectangle<int>(0, 0, rect.GetWidth(), rect.GetHeight()));
    g_gfx->SetPipeline(data.copy_pipeline.get());
    g_gfx->SetTexture(0, src_texture);
    g_gfx->SetSamplerState(0, depth ? RenderState::GetPointSamplerState() :
                                      RenderState::GetLinearSamplerState());
    g_gfx->Draw(0, 3);

    // Copy from EFB or copy texture to staging texture.
    // No need to call FinishedRendering() here because CopyFromTexture() transitions.
    data.readback_texture->CopyFromTexture(
        data.texture.get(), MathUtil::Rectangle<int>(0, 0, rect.GetWidth(), rect.GetHeight()), 0, 0,
        rect);

    g_gfx->EndUtilityDrawing();
  }
  else
  {
    data.readback_texture->CopyFromTexture(src_texture, rect, 0, 0, rect);
  }

  // Wait until the copy is complete.
  if (!async)
  {
    data.readback_texture->Flush();
    data.needs_flush = false;
  }
  else
  {
    data.needs_flush = true;
  }
  data.has_active_tiles = true;
  data.out_of_date = false;
  data.tiles[tile_index].present = true;
}

void FramebufferManager::ClearEFB(const MathUtil::Rectangle<int>& rc, bool color_enable,
                                  bool alpha_enable, bool z_enable, u32 color, u32 z)
{
  FlushEFBPokes();
  FlagPeekCacheAsOutOfDate();

  // Native -> EFB coordinates
  MathUtil::Rectangle<int> target_rc = ConvertEFBRectangle(rc);
  target_rc = g_gfx->ConvertFramebufferRectangle(target_rc, m_efb_framebuffer.get());
  target_rc.ClampUL(0, 0, m_efb_framebuffer->GetWidth(), m_efb_framebuffer->GetWidth());

  // Determine whether the EFB has an alpha channel. If it doesn't, we can clear the alpha
  // channel to 0xFF.
  // On backends that don't allow masking Alpha clears, this allows us to use the fast path
  // almost all the time
  if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16 ||
      bpmem.zcontrol.pixel_format == PixelFormat::RGB8_Z24 ||
      bpmem.zcontrol.pixel_format == PixelFormat::Z24)
  {
    // Force alpha writes, and clear the alpha channel.
    alpha_enable = true;
    color &= 0x00FFFFFF;
  }

  g_gfx->ClearRegion(target_rc, color_enable, alpha_enable, z_enable, color, z);

  // Scissor rect must be restored.
  BPFunctions::SetScissorAndViewport();
}

bool FramebufferManager::CompileClearPipelines()
{
  auto vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateClearVertexShader(),
      "Clear vertex shader");
  if (!vertex_shader)
    return false;

  AbstractPipelineConfig config;
  config.vertex_format = nullptr;
  config.vertex_shader = vertex_shader.get();
  config.geometry_shader = IsEFBStereo() ? g_shader_cache->GetColorGeometryShader() : nullptr;
  config.pixel_shader = g_shader_cache->GetColorPixelShader();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetAlwaysWriteDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = GetEFBFramebufferState();
  config.usage = AbstractPipelineUsage::Utility;

  for (u32 color_enable = 0; color_enable < 2; color_enable++)
  {
    config.blending_state.colorupdate = color_enable != 0;
    for (u32 alpha_enable = 0; alpha_enable < 2; alpha_enable++)
    {
      config.blending_state.alphaupdate = alpha_enable != 0;
      for (u32 depth_enable = 0; depth_enable < 2; depth_enable++)
      {
        config.depth_state.testenable = depth_enable != 0;
        config.depth_state.updateenable = depth_enable != 0;

        m_clear_pipelines[color_enable][alpha_enable][depth_enable] = g_gfx->CreatePipeline(config);
        if (!m_clear_pipelines[color_enable][alpha_enable][depth_enable])
          return false;
      }
    }
  }

  return true;
}

void FramebufferManager::DestroyClearPipelines()
{
  for (u32 color_enable = 0; color_enable < 2; color_enable++)
  {
    for (u32 alpha_enable = 0; alpha_enable < 2; alpha_enable++)
    {
      for (u32 depth_enable = 0; depth_enable < 2; depth_enable++)
      {
        m_clear_pipelines[color_enable][alpha_enable][depth_enable].reset();
      }
    }
  }
}

AbstractPipeline* FramebufferManager::GetClearPipeline(bool colorEnable, bool alphaEnable,
                                                       bool zEnable) const
{
  return m_clear_pipelines[colorEnable][alphaEnable][zEnable].get();
}

void FramebufferManager::PokeEFBColor(u32 x, u32 y, u32 color)
{
  // Flush if we exceeded the number of vertices per batch.
  if ((m_color_poke_vertices.size() + 6) > MAX_POKE_VERTICES)
    FlushEFBPokes();

  CreatePokeVertices(&m_color_poke_vertices, x, y, 0.0f, color);

  // See comment above for reasoning for lower-left coordinates.
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
    y = EFB_HEIGHT - 1 - y;

  // Update the peek cache if it's valid, since we know the color of the pixel now.
  u32 tile_index;
  if (IsEFBCacheTilePresent(false, x, y, &tile_index))
    m_efb_color_cache.readback_texture->WriteTexel(x, y, &color);
}

void FramebufferManager::PokeEFBDepth(u32 x, u32 y, float depth)
{
  // Flush if we exceeded the number of vertices per batch.
  if ((m_depth_poke_vertices.size() + 6) > MAX_POKE_VERTICES)
    FlushEFBPokes();

  CreatePokeVertices(&m_depth_poke_vertices, x, y, depth, 0);

  // See comment above for reasoning for lower-left coordinates.
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
    y = EFB_HEIGHT - 1 - y;

  // Update the peek cache if it's valid, since we know the color of the pixel now.
  u32 tile_index;
  if (IsEFBCacheTilePresent(true, x, y, &tile_index))
    m_efb_depth_cache.readback_texture->WriteTexel(x, y, &depth);
}

void FramebufferManager::CreatePokeVertices(std::vector<EFBPokeVertex>* destination_list, u32 x,
                                            u32 y, float z, u32 color)
{
  const float cs_pixel_width = 1.0f / EFB_WIDTH * 2.0f;
  const float cs_pixel_height = 1.0f / EFB_HEIGHT * 2.0f;
  if (g_ActiveConfig.backend_info.bSupportsLargePoints)
  {
    // GPU will expand the point to a quad.
    const float cs_x = (static_cast<float>(x) + 0.5f) * cs_pixel_width - 1.0f;
    const float cs_y = 1.0f - (static_cast<float>(y) + 0.5f) * cs_pixel_height;
    const float point_size = static_cast<float>(GetEFBScale());
    destination_list->push_back({{cs_x, cs_y, z, point_size}, color});
    return;
  }

  // Generate quad from the single point (clip-space coordinates).
  const float x1 = static_cast<float>(x) * cs_pixel_width - 1.0f;
  const float y1 = 1.0f - static_cast<float>(y) * cs_pixel_height;
  const float x2 = x1 + cs_pixel_width;
  const float y2 = y1 - cs_pixel_height;
  destination_list->push_back({{x1, y1, z, 1.0f}, color});
  destination_list->push_back({{x2, y1, z, 1.0f}, color});
  destination_list->push_back({{x1, y2, z, 1.0f}, color});
  destination_list->push_back({{x1, y2, z, 1.0f}, color});
  destination_list->push_back({{x2, y1, z, 1.0f}, color});
  destination_list->push_back({{x2, y2, z, 1.0f}, color});
}

void FramebufferManager::FlushEFBPokes()
{
  if (!m_color_poke_vertices.empty())
  {
    DrawPokeVertices(m_color_poke_vertices.data(), static_cast<u32>(m_color_poke_vertices.size()),
                     m_color_poke_pipeline.get());
    m_color_poke_vertices.clear();
  }

  if (!m_depth_poke_vertices.empty())
  {
    DrawPokeVertices(m_depth_poke_vertices.data(), static_cast<u32>(m_depth_poke_vertices.size()),
                     m_depth_poke_pipeline.get());
    m_depth_poke_vertices.clear();
  }
}

void FramebufferManager::DrawPokeVertices(const EFBPokeVertex* vertices, u32 vertex_count,
                                          const AbstractPipeline* pipeline)
{
  // Copy to vertex buffer.
  g_gfx->BeginUtilityDrawing();
  u32 base_vertex, base_index;
  g_vertex_manager->UploadUtilityVertices(vertices, sizeof(EFBPokeVertex),
                                          static_cast<u32>(vertex_count), nullptr, 0, &base_vertex,
                                          &base_index);

  // Now we can draw.
  g_gfx->SetViewportAndScissor(m_efb_framebuffer->GetRect());
  g_gfx->SetPipeline(pipeline);
  g_gfx->Draw(base_vertex, vertex_count);
  g_gfx->EndUtilityDrawing();
}

bool FramebufferManager::CompilePokePipelines()
{
  PortableVertexDeclaration vtx_decl = {};
  vtx_decl.position.enable = true;
  vtx_decl.position.type = ComponentFormat::Float;
  vtx_decl.position.components = 4;
  vtx_decl.position.integer = false;
  vtx_decl.position.offset = offsetof(EFBPokeVertex, position);
  vtx_decl.colors[0].enable = true;
  vtx_decl.colors[0].type = ComponentFormat::UByte;
  vtx_decl.colors[0].components = 4;
  vtx_decl.colors[0].integer = false;
  vtx_decl.colors[0].offset = offsetof(EFBPokeVertex, color);
  vtx_decl.stride = sizeof(EFBPokeVertex);

  m_poke_vertex_format = g_gfx->CreateNativeVertexFormat(vtx_decl);
  if (!m_poke_vertex_format)
    return false;

  auto poke_vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateEFBPokeVertexShader(),
      "EFB poke vertex shader");
  if (!poke_vertex_shader)
    return false;

  AbstractPipelineConfig config = {};
  config.vertex_format = m_poke_vertex_format.get();
  config.vertex_shader = poke_vertex_shader.get();
  config.geometry_shader = IsEFBStereo() ? g_shader_cache->GetColorGeometryShader() : nullptr;
  config.pixel_shader = g_shader_cache->GetColorPixelShader();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(
      g_ActiveConfig.backend_info.bSupportsLargePoints ? PrimitiveType::Points :
                                                         PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = GetEFBFramebufferState();
  config.usage = AbstractPipelineUsage::Utility;
  m_color_poke_pipeline = g_gfx->CreatePipeline(config);
  if (!m_color_poke_pipeline)
    return false;

  // Turn off color writes, depth writes on for depth pokes.
  config.depth_state = RenderState::GetAlwaysWriteDepthState();
  config.blending_state = RenderState::GetNoColorWriteBlendState();
  m_depth_poke_pipeline = g_gfx->CreatePipeline(config);
  if (!m_depth_poke_pipeline)
    return false;

  return true;
}

void FramebufferManager::DestroyPokePipelines()
{
  m_depth_poke_pipeline.reset();
  m_color_poke_pipeline.reset();
  m_poke_vertex_format.reset();
}

void FramebufferManager::DoState(PointerWrap& p)
{
  FlushEFBPokes();
  p.Do(m_prev_efb_format);

  bool save_efb_state = Config::Get(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE);
  p.Do(save_efb_state);
  if (!save_efb_state)
    return;

  if (p.IsWriteMode() || p.IsMeasureMode())
    DoSaveState(p);
  else
    DoLoadState(p);
}

void FramebufferManager::DoSaveState(PointerWrap& p)
{
  // For multisampling, we need to resolve first before we can save.
  // This won't be bit-exact when loading, which could cause interesting rendering side-effects for
  // a frame. But whatever, MSAA doesn't exactly behave that well anyway.
  AbstractTexture* color_texture = ResolveEFBColorTexture(m_efb_color_texture->GetRect());
  AbstractTexture* depth_texture = ResolveEFBDepthTexture(m_efb_depth_texture->GetRect(), true);

  // We don't want to save these as rendertarget textures, just the data itself when deserializing.
  const TextureConfig color_texture_config(
      color_texture->GetWidth(), color_texture->GetHeight(), color_texture->GetLevels(),
      color_texture->GetLayers(), 1, GetEFBColorFormat(), 0, AbstractTextureType::Texture_2DArray);
  g_texture_cache->SerializeTexture(color_texture, color_texture_config, p);

  const TextureConfig depth_texture_config(depth_texture->GetWidth(), depth_texture->GetHeight(),
                                           depth_texture->GetLevels(), depth_texture->GetLayers(),
                                           1, GetEFBDepthCopyFormat(), 0,
                                           AbstractTextureType::Texture_2DArray);
  g_texture_cache->SerializeTexture(depth_texture, depth_texture_config, p);
}

void FramebufferManager::DoLoadState(PointerWrap& p)
{
  // Invalidate any peek cache tiles.
  InvalidatePeekCache(true);

  // Deserialize the color and depth textures. This could fail.
  auto color_tex = g_texture_cache->DeserializeTexture(p);
  auto depth_tex = g_texture_cache->DeserializeTexture(p);

  // If the stereo mode is different in the save state, throw it away.
  if (!color_tex || !depth_tex ||
      color_tex->texture->GetLayers() != m_efb_color_texture->GetLayers())
  {
    WARN_LOG_FMT(VIDEO, "Failed to deserialize EFB contents. Clearing instead.");
    g_gfx->SetAndClearFramebuffer(m_efb_framebuffer.get(), {{0.0f, 0.0f, 0.0f, 0.0f}},
                                  g_ActiveConfig.backend_info.bSupportsReversedDepthRange ? 1.0f :
                                                                                            0.0f);
    return;
  }

  // Size differences are okay here, since the linear filtering will downscale/upscale it.
  // Depth buffer is always point sampled, since we don't want to interpolate depth values.
  const bool rescale = color_tex->texture->GetWidth() != m_efb_color_texture->GetWidth() ||
                       color_tex->texture->GetHeight() != m_efb_color_texture->GetHeight();

  // Draw the deserialized textures over the EFB.
  g_gfx->BeginUtilityDrawing();
  g_gfx->SetAndDiscardFramebuffer(m_efb_framebuffer.get());
  g_gfx->SetViewportAndScissor(m_efb_framebuffer->GetRect());
  g_gfx->SetPipeline(m_efb_restore_pipeline.get());
  g_gfx->SetTexture(0, color_tex->texture.get());
  g_gfx->SetTexture(1, depth_tex->texture.get());
  g_gfx->SetSamplerState(0, rescale ? RenderState::GetLinearSamplerState() :
                                      RenderState::GetPointSamplerState());
  g_gfx->SetSamplerState(1, RenderState::GetPointSamplerState());
  g_gfx->Draw(0, 3);
  g_gfx->EndUtilityDrawing();
}
