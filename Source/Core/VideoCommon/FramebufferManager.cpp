// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FramebufferManager.h"
#include <memory>
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/VertexManagerBase.h"

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

// Maximum number of pixels poked in one batch * 6
constexpr size_t MAX_POKE_VERTICES = 32768;

std::unique_ptr<FramebufferManager> g_framebuffer_manager;

FramebufferManager::FramebufferManager() = default;

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
    PanicAlert("Failed to create EFB framebuffer");
    return false;
  }

  m_efb_cache_tile_size = static_cast<u32>(std::max(g_ActiveConfig.iEFBAccessTileSize, 0));
  if (!CreateReadbackFramebuffer())
  {
    PanicAlert("Failed to create EFB readback framebuffer");
    return false;
  }

  if (!CompileReadbackPipelines())
  {
    PanicAlert("Failed to compile EFB readback pipelines");
    return false;
  }

  if (!CompileConversionPipelines())
  {
    PanicAlert("Failed to compile EFB conversion pipelines");
    return false;
  }

  if (!CompileClearPipelines())
  {
    PanicAlert("Failed to compile EFB clear pipelines");
    return false;
  }

  if (!CompilePokePipelines())
  {
    PanicAlert("Failed to compile EFB poke pipelines");
    return false;
  }

  return true;
}

void FramebufferManager::RecreateEFBFramebuffer()
{
  FlushEFBPokes();
  InvalidatePeekCache(true);

  DestroyReadbackFramebuffer();
  DestroyEFBFramebuffer();
  if (!CreateEFBFramebuffer() || !CreateReadbackFramebuffer())
    PanicAlert("Failed to recreate EFB framebuffer");
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
    PanicAlert("Failed to recompile EFB pipelines");
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

static u32 CalculateEFBLayers()
{
  return 1;
}

TextureConfig FramebufferManager::GetEFBColorTextureConfig()
{
  return TextureConfig(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(), 1,
                       CalculateEFBLayers(), g_ActiveConfig.iMultisamples, GetEFBColorFormat(),
                       AbstractTextureFlag_RenderTarget);
}

TextureConfig FramebufferManager::GetEFBDepthTextureConfig()
{
  return TextureConfig(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(), 1,
                       CalculateEFBLayers(), g_ActiveConfig.iMultisamples, GetEFBDepthFormat(),
                       AbstractTextureFlag_RenderTarget);
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

bool FramebufferManager::CreateEFBFramebuffer()
{
  const TextureConfig efb_color_texture_config = GetEFBColorTextureConfig();
  const TextureConfig efb_depth_texture_config = GetEFBDepthTextureConfig();

  // We need a second texture to swap with for changing pixel formats
  m_efb_color_texture = g_renderer->CreateTexture(efb_color_texture_config);
  m_efb_depth_texture = g_renderer->CreateTexture(efb_depth_texture_config);
  m_efb_convert_color_texture = g_renderer->CreateTexture(efb_color_texture_config);
  if (!m_efb_color_texture || !m_efb_depth_texture || !m_efb_convert_color_texture)
    return false;

  m_efb_framebuffer =
      g_renderer->CreateFramebuffer(m_efb_color_texture.get(), m_efb_depth_texture.get());
  m_efb_convert_framebuffer =
      g_renderer->CreateFramebuffer(m_efb_convert_color_texture.get(), m_efb_depth_texture.get());
  if (!m_efb_framebuffer || !m_efb_convert_framebuffer)
    return false;

  // Create resolved textures if MSAA is on
  if (g_ActiveConfig.MultisamplingEnabled())
  {
    m_efb_resolve_color_texture = g_renderer->CreateTexture(
        TextureConfig(efb_color_texture_config.width, efb_color_texture_config.height, 1,
                      efb_color_texture_config.layers, 1, efb_color_texture_config.format, 0));
    m_efb_depth_resolve_texture = g_renderer->CreateTexture(TextureConfig(
        efb_depth_texture_config.width, efb_depth_texture_config.height, 1,
        efb_depth_texture_config.layers, 1,
        AbstractTexture::GetColorFormatForDepthFormat(efb_depth_texture_config.format),
        AbstractTextureFlag_RenderTarget));
    if (!m_efb_resolve_color_texture || !m_efb_depth_resolve_texture)
      return false;

    m_efb_depth_resolve_framebuffer =
        g_renderer->CreateFramebuffer(m_efb_depth_resolve_texture.get(), nullptr);
    if (!m_efb_depth_resolve_framebuffer)
      return false;
  }

  // Clear the renderable textures out.
  g_renderer->SetAndClearFramebuffer(
      m_efb_framebuffer.get(), {{0.0f, 0.0f, 0.0f, 0.0f}},
      g_ActiveConfig.backend_info.bSupportsReversedDepthRange ? 1.0f : 0.0f);
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
  g_renderer->SetFramebuffer(m_efb_framebuffer.get());
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
  for (u32 layer = 0; layer < GetEFBLayers(); layer++)
  {
    m_efb_resolve_color_texture->ResolveFromTexture(m_efb_color_texture.get(), clamped_region,
                                                    layer, 0);
  }

  m_efb_resolve_color_texture->FinishedRendering();
  return m_efb_resolve_color_texture.get();
}

AbstractTexture* FramebufferManager::ResolveEFBDepthTexture(const MathUtil::Rectangle<int>& region)
{
  if (!IsEFBMultisampled())
    return m_efb_depth_texture.get();

  // It's not valid to resolve an out-of-range rectangle.
  MathUtil::Rectangle<int> clamped_region = region;
  clamped_region.ClampUL(0, 0, GetEFBWidth(), GetEFBHeight());

  m_efb_depth_texture->FinishedRendering();
  g_renderer->BeginUtilityDrawing();
  g_renderer->SetAndDiscardFramebuffer(m_efb_depth_resolve_framebuffer.get());
  g_renderer->SetPipeline(m_efb_depth_resolve_pipeline.get());
  g_renderer->SetTexture(0, m_efb_depth_texture.get());
  g_renderer->SetSamplerState(0, RenderState::GetPointSamplerState());
  g_renderer->SetViewportAndScissor(clamped_region);
  g_renderer->Draw(0, 3);
  m_efb_depth_resolve_texture->FinishedRendering();
  g_renderer->EndUtilityDrawing();

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
  g_renderer->BeginUtilityDrawing();
  g_renderer->SetFramebuffer(m_efb_convert_framebuffer.get());
  g_renderer->SetViewportAndScissor(m_efb_framebuffer->GetRect());
  g_renderer->SetPipeline(m_format_conversion_pipelines[static_cast<u32>(convtype)].get());
  g_renderer->SetTexture(0, m_efb_color_texture.get());
  g_renderer->Draw(0, 3);

  // And swap the framebuffers around, so we do new drawing to the converted framebuffer.
  std::swap(m_efb_color_texture, m_efb_convert_color_texture);
  std::swap(m_efb_framebuffer, m_efb_convert_framebuffer);
  g_renderer->EndUtilityDrawing();
  InvalidatePeekCache(true);
  return true;
}

bool FramebufferManager::CompileConversionPipelines()
{
  for (u32 i = 0; i < NUM_EFB_REINTERPRET_TYPES; i++)
  {
    std::unique_ptr<AbstractShader> pixel_shader = g_renderer->CreateShaderFromSource(
        ShaderStage::Pixel, FramebufferShaderGen::GenerateFormatConversionShader(
                                static_cast<EFBReinterpretType>(i), GetEFBSamples()));
    if (!pixel_shader)
      return false;

    AbstractPipelineConfig config = {};
    config.vertex_shader = g_shader_cache->GetScreenQuadVertexShader();
    config.geometry_shader = nullptr;
    config.pixel_shader = pixel_shader.get();
    config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
    config.depth_state = RenderState::GetNoDepthTestingDepthState();
    config.blending_state = RenderState::GetNoBlendingBlendState();
    config.framebuffer_state = GetEFBFramebufferState();
    config.usage = AbstractPipelineUsage::Utility;
    m_format_conversion_pipelines[i] = g_renderer->CreatePipeline(config);
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
  if (m_efb_cache_tile_size == 0)
  {
    *tile_index = 0;
    return data.valid;
  }
  else
  {
    *tile_index =
        ((y / m_efb_cache_tile_size) * m_efb_cache_tiles_wide) + (x / m_efb_cache_tile_size);
    return data.valid && data.tiles[*tile_index];
  }
}

MathUtil::Rectangle<int> FramebufferManager::GetEFBCacheTileRect(u32 tile_index) const
{
  if (m_efb_cache_tile_size == 0)
    return MathUtil::Rectangle<int>(0, 0, EFB_WIDTH, EFB_HEIGHT);

  const u32 tile_y = tile_index / m_efb_cache_tiles_wide;
  const u32 tile_x = tile_index % m_efb_cache_tiles_wide;
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
    PanicAlert("Failed to create EFB readback framebuffers");
}

void FramebufferManager::InvalidatePeekCache(bool forced)
{
  if (forced || m_efb_color_cache.out_of_date)
  {
    if (m_efb_color_cache.valid)
      std::fill(m_efb_color_cache.tiles.begin(), m_efb_color_cache.tiles.end(), false);

    m_efb_color_cache.valid = false;
    m_efb_color_cache.out_of_date = false;
  }
  if (forced || m_efb_depth_cache.out_of_date)
  {
    if (m_efb_depth_cache.valid)
      std::fill(m_efb_depth_cache.tiles.begin(), m_efb_depth_cache.tiles.end(), false);

    m_efb_depth_cache.valid = false;
    m_efb_depth_cache.out_of_date = false;
  }
}

void FramebufferManager::FlagPeekCacheAsOutOfDate()
{
  if (m_efb_color_cache.valid)
    m_efb_color_cache.out_of_date = true;
  if (m_efb_depth_cache.valid)
    m_efb_depth_cache.out_of_date = true;

  if (!g_ActiveConfig.bEFBAccessDeferInvalidation)
    InvalidatePeekCache();
}

bool FramebufferManager::CompileReadbackPipelines()
{
  AbstractPipelineConfig config = {};
  config.vertex_shader = g_shader_cache->GetTextureCopyVertexShader();
  config.geometry_shader = nullptr;
  config.pixel_shader = g_shader_cache->GetTextureCopyPixelShader();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(GetEFBColorFormat());
  config.usage = AbstractPipelineUsage::Utility;
  m_efb_color_cache.copy_pipeline = g_renderer->CreatePipeline(config);
  if (!m_efb_color_cache.copy_pipeline)
    return false;

  // same for depth, except different format
  config.framebuffer_state.color_texture_format =
      AbstractTexture::GetColorFormatForDepthFormat(GetEFBDepthFormat());
  m_efb_depth_cache.copy_pipeline = g_renderer->CreatePipeline(config);
  if (!m_efb_depth_cache.copy_pipeline)
    return false;

  if (IsEFBMultisampled())
  {
    auto depth_resolve_shader = g_renderer->CreateShaderFromSource(
        ShaderStage::Pixel, FramebufferShaderGen::GenerateResolveDepthPixelShader(GetEFBSamples()));
    if (!depth_resolve_shader)
      return false;

    config.pixel_shader = depth_resolve_shader.get();
    m_efb_depth_resolve_pipeline = g_renderer->CreatePipeline(config);
    if (!m_efb_depth_resolve_pipeline)
      return false;
  }

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
  // Since we can't partially copy from a depth buffer directly to the staging texture in D3D, we
  // use an intermediate buffer to avoid copying the whole texture.
  if ((IsUsingTiledEFBCache() && !g_ActiveConfig.backend_info.bSupportsPartialDepthCopies) ||
      g_renderer->IsScaledEFB())
  {
    const TextureConfig color_config(IsUsingTiledEFBCache() ? m_efb_cache_tile_size : EFB_WIDTH,
                                     IsUsingTiledEFBCache() ? m_efb_cache_tile_size : EFB_HEIGHT, 1,
                                     1, 1, GetEFBColorFormat(), AbstractTextureFlag_RenderTarget);
    const TextureConfig depth_config(
        color_config.width, color_config.height, 1, 1, 1,
        AbstractTexture::GetColorFormatForDepthFormat(GetEFBDepthFormat()),
        AbstractTextureFlag_RenderTarget);

    m_efb_color_cache.texture = g_renderer->CreateTexture(color_config);
    m_efb_depth_cache.texture = g_renderer->CreateTexture(depth_config);
    if (!m_efb_color_cache.texture || !m_efb_depth_cache.texture)
      return false;

    m_efb_color_cache.framebuffer =
        g_renderer->CreateFramebuffer(m_efb_color_cache.texture.get(), nullptr);
    m_efb_depth_cache.framebuffer =
        g_renderer->CreateFramebuffer(m_efb_depth_cache.texture.get(), nullptr);
    if (!m_efb_color_cache.framebuffer || !m_efb_depth_cache.framebuffer)
      return false;
  }

  // Staging texture use the full EFB dimensions, as this is the buffer for the whole cache.
  m_efb_color_cache.readback_texture = g_renderer->CreateStagingTexture(
      StagingTextureType::Mutable,
      TextureConfig(EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, GetEFBColorFormat(), 0));
  m_efb_depth_cache.readback_texture = g_renderer->CreateStagingTexture(
      StagingTextureType::Mutable,
      TextureConfig(EFB_WIDTH, EFB_HEIGHT, 1, 1, 1,
                    AbstractTexture::GetColorFormatForDepthFormat(GetEFBDepthFormat()), 0));
  if (!m_efb_color_cache.readback_texture || !m_efb_depth_cache.readback_texture)
    return false;

  if (IsUsingTiledEFBCache())
  {
    const u32 tiles_wide = ((EFB_WIDTH + (m_efb_cache_tile_size - 1)) / m_efb_cache_tile_size);
    const u32 tiles_high = ((EFB_HEIGHT + (m_efb_cache_tile_size - 1)) / m_efb_cache_tile_size);
    const u32 total_tiles = tiles_wide * tiles_high;
    m_efb_color_cache.tiles.resize(total_tiles);
    std::fill(m_efb_color_cache.tiles.begin(), m_efb_color_cache.tiles.end(), false);
    m_efb_depth_cache.tiles.resize(total_tiles);
    std::fill(m_efb_depth_cache.tiles.begin(), m_efb_depth_cache.tiles.end(), false);
    m_efb_cache_tiles_wide = tiles_wide;
  }

  return true;
}

void FramebufferManager::DestroyReadbackFramebuffer()
{
  auto DestroyCache = [](EFBCacheData& data) {
    data.readback_texture.reset();
    data.framebuffer.reset();
    data.texture.reset();
    data.valid = false;
  };
  DestroyCache(m_efb_color_cache);
  DestroyCache(m_efb_depth_cache);
}

void FramebufferManager::PopulateEFBCache(bool depth, u32 tile_index)
{
  g_vertex_manager->OnCPUEFBAccess();

  // Force the path through the intermediate texture, as we can't do an image copy from a depth
  // buffer directly to a staging texture (must be the whole resource).
  const bool force_intermediate_copy =
      depth && !g_ActiveConfig.backend_info.bSupportsPartialDepthCopies && IsUsingTiledEFBCache();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  EFBCacheData& data = depth ? m_efb_depth_cache : m_efb_color_cache;
  const MathUtil::Rectangle<int> rect = GetEFBCacheTileRect(tile_index);
  const MathUtil::Rectangle<int> native_rect = g_renderer->ConvertEFBRectangle(rect);
  AbstractTexture* src_texture =
      depth ? ResolveEFBDepthTexture(native_rect) : ResolveEFBColorTexture(native_rect);
  if (g_renderer->IsScaledEFB() || force_intermediate_copy)
  {
    // Downsample from internal resolution to 1x.
    // TODO: This won't produce correct results at IRs above 2x. More samples are required.
    // This is the same issue as with EFB copies.
    src_texture->FinishedRendering();
    g_renderer->BeginUtilityDrawing();

    const float rcp_src_width = 1.0f / m_efb_framebuffer->GetWidth();
    const float rcp_src_height = 1.0f / m_efb_framebuffer->GetHeight();
    const std::array<float, 4> uniforms = {
        {native_rect.left * rcp_src_width, native_rect.top * rcp_src_height,
         native_rect.GetWidth() * rcp_src_width, native_rect.GetHeight() * rcp_src_height}};
    g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

    // Viewport will not be TILE_SIZExTILE_SIZE for the last row of tiles, assuming a tile size of
    // 64, because 528 is not evenly divisible by 64.
    g_renderer->SetAndDiscardFramebuffer(data.framebuffer.get());
    g_renderer->SetViewportAndScissor(
        MathUtil::Rectangle<int>(0, 0, rect.GetWidth(), rect.GetHeight()));
    g_renderer->SetPipeline(data.copy_pipeline.get());
    g_renderer->SetTexture(0, src_texture);
    g_renderer->SetSamplerState(0, depth ? RenderState::GetPointSamplerState() :
                                           RenderState::GetLinearSamplerState());
    g_renderer->Draw(0, 3);

    // Copy from EFB or copy texture to staging texture.
    // No need to call FinishedRendering() here because CopyFromTexture() transitions.
    data.readback_texture->CopyFromTexture(
        data.texture.get(), MathUtil::Rectangle<int>(0, 0, rect.GetWidth(), rect.GetHeight()), 0, 0,
        rect);

    g_renderer->EndUtilityDrawing();
  }
  else
  {
    data.readback_texture->CopyFromTexture(src_texture, rect, 0, 0, rect);
  }

  // Wait until the copy is complete.
  data.readback_texture->Flush();
  data.valid = true;
  data.out_of_date = false;
  if (IsUsingTiledEFBCache())
    data.tiles[tile_index] = true;
}

void FramebufferManager::ClearEFB(const MathUtil::Rectangle<int>& rc, bool clear_color,
                                  bool clear_alpha, bool clear_z, u32 color, u32 z)
{
  FlushEFBPokes();
  FlagPeekCacheAsOutOfDate();
  g_renderer->BeginUtilityDrawing();

  // Set up uniforms.
  struct Uniforms
  {
    float clear_color[4];
    float clear_depth;
    float padding1, padding2, padding3;
  };
  static_assert(std::is_standard_layout<Uniforms>::value);
  Uniforms uniforms = {{static_cast<float>((color >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((color >> 8) & 0xFF) / 255.0f,
                        static_cast<float>((color >> 0) & 0xFF) / 255.0f,
                        static_cast<float>((color >> 24) & 0xFF) / 255.0f},
                       static_cast<float>(z & 0xFFFFFF) / 16777216.0f};
  if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
    uniforms.clear_depth = 1.0f - uniforms.clear_depth;
  g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

  const auto target_rc = g_renderer->ConvertFramebufferRectangle(
      g_renderer->ConvertEFBRectangle(rc), m_efb_framebuffer.get());
  g_renderer->SetPipeline(m_efb_clear_pipelines[clear_color][clear_alpha][clear_z].get());
  g_renderer->SetViewportAndScissor(target_rc);
  g_renderer->Draw(0, 3);
  g_renderer->EndUtilityDrawing();
}

bool FramebufferManager::CompileClearPipelines()
{
  auto vertex_shader = g_renderer->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateClearVertexShader());
  if (!vertex_shader)
    return false;

  AbstractPipelineConfig config;
  config.vertex_format = nullptr;
  config.vertex_shader = vertex_shader.get();
  config.geometry_shader = nullptr;
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

        m_efb_clear_pipelines[color_enable][alpha_enable][depth_enable] =
            g_renderer->CreatePipeline(config);
        if (!m_efb_clear_pipelines[color_enable][alpha_enable][depth_enable])
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
        m_efb_clear_pipelines[color_enable][alpha_enable][depth_enable].reset();
      }
    }
  }
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
    const float point_size = static_cast<float>(g_renderer->GetEFBScale());
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
  g_renderer->BeginUtilityDrawing();
  u32 base_vertex, base_index;
  g_vertex_manager->UploadUtilityVertices(vertices, sizeof(EFBPokeVertex),
                                          static_cast<u32>(vertex_count), nullptr, 0, &base_vertex,
                                          &base_index);

  // Now we can draw.
  g_renderer->SetViewportAndScissor(m_efb_framebuffer->GetRect());
  g_renderer->SetPipeline(pipeline);
  g_renderer->Draw(base_vertex, vertex_count);
  g_renderer->EndUtilityDrawing();
}

bool FramebufferManager::CompilePokePipelines()
{
  PortableVertexDeclaration vtx_decl = {};
  vtx_decl.position.enable = true;
  vtx_decl.position.type = VAR_FLOAT;
  vtx_decl.position.components = 4;
  vtx_decl.position.integer = false;
  vtx_decl.position.offset = offsetof(EFBPokeVertex, position);
  vtx_decl.colors[0].enable = true;
  vtx_decl.colors[0].type = VAR_UNSIGNED_BYTE;
  vtx_decl.colors[0].components = 4;
  vtx_decl.colors[0].integer = false;
  vtx_decl.colors[0].offset = offsetof(EFBPokeVertex, color);
  vtx_decl.stride = sizeof(EFBPokeVertex);

  m_poke_vertex_format = g_renderer->CreateNativeVertexFormat(vtx_decl);
  if (!m_poke_vertex_format)
    return false;

  auto poke_vertex_shader = g_renderer->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateEFBPokeVertexShader());
  if (!poke_vertex_shader)
    return false;

  AbstractPipelineConfig config = {};
  config.vertex_format = m_poke_vertex_format.get();
  config.vertex_shader = poke_vertex_shader.get();
  config.geometry_shader = nullptr;
  config.pixel_shader = g_shader_cache->GetColorPixelShader();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(
      g_ActiveConfig.backend_info.bSupportsLargePoints ? PrimitiveType::Points :
                                                         PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = GetEFBFramebufferState();
  config.usage = AbstractPipelineUsage::Utility;
  m_color_poke_pipeline = g_renderer->CreatePipeline(config);
  if (!m_color_poke_pipeline)
    return false;

  // Turn off color writes, depth writes on for depth pokes.
  config.depth_state = RenderState::GetAlwaysWriteDepthState();
  config.blending_state = RenderState::GetNoColorWriteBlendState();
  m_depth_poke_pipeline = g_renderer->CreatePipeline(config);
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
