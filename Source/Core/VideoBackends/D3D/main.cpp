// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/PerfQuery.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/SwapChain.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3DCommon/Common.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
std::string VideoBackend::GetName() const
{
  return "D3D";
}

std::string VideoBackend::GetDisplayName() const
{
  return _trans("Direct3D 11");
}

void VideoBackend::InitBackendInfo()
{
  if (!D3DCommon::LoadLibraries())
    return;

  FillBackendInfo();
  D3DCommon::UnloadLibraries();
}

void VideoBackend::FillBackendInfo()
{
  g_Config.backend_info.api_type = APIType::D3D;
  g_Config.backend_info.MaxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  g_Config.backend_info.bUsesLowerLeftOrigin = false;
  g_Config.backend_info.bSupportsExclusiveFullscreen = true;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsOversizedViewports = false;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupportsPostProcessing = true;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsReversedDepthRange = false;
  g_Config.backend_info.bSupportsLogicOp = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = true;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsCopyToVram = true;
  g_Config.backend_info.bSupportsLargePoints = false;
  g_Config.backend_info.bSupportsPartialDepthCopies = false;
  g_Config.backend_info.bSupportsBitfield = false;
  g_Config.backend_info.bSupportsDynamicSamplerIndexing = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;
  g_Config.backend_info.bSupportsFramebufferFetch = false;
  g_Config.backend_info.bSupportsBackgroundCompiling = true;
  g_Config.backend_info.bSupportsST3CTextures = true;
  g_Config.backend_info.bSupportsBPTCTextures = true;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsBBox = true;
  g_Config.backend_info.bSupportsFragmentStoresAndAtomics = true;
  g_Config.backend_info.bSupportsGSInstancing = true;
  g_Config.backend_info.bSupportsSSAA = true;
  g_Config.backend_info.bSupportsShaderBinaries = true;
  g_Config.backend_info.bSupportsPipelineCacheData = false;

  g_Config.backend_info.Adapters = D3DCommon::GetAdapterNames();
  g_Config.backend_info.AAModes = D3D::GetAAModes(g_Config.iAdapter);

  // Override optional features if we are actually booting.
  if (D3D::device)
  {
    g_Config.backend_info.bSupportsST3CTextures =
        D3D::SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
        D3D::SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
        D3D::SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
    g_Config.backend_info.bSupportsBPTCTextures = D3D::SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);

    // Features only supported with a FL11.0+ device.
    const bool shader_model_5_supported = D3D::feature_level >= D3D_FEATURE_LEVEL_11_0;
    g_Config.backend_info.bSupportsEarlyZ = shader_model_5_supported;
    g_Config.backend_info.bSupportsBBox = shader_model_5_supported;
    g_Config.backend_info.bSupportsFragmentStoresAndAtomics = shader_model_5_supported;
    g_Config.backend_info.bSupportsGSInstancing = shader_model_5_supported;
    g_Config.backend_info.bSupportsSSAA = shader_model_5_supported;
    g_Config.backend_info.bSupportsGPUTextureDecoding = shader_model_5_supported;
  }
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  if (!D3D::Create(g_Config.iAdapter, g_Config.bEnableValidationLayer))
    return false;

  FillBackendInfo();
  InitializeShared();

  std::unique_ptr<SwapChain> swap_chain;
  if (wsi.render_surface && !(swap_chain = SwapChain::Create(wsi)))
  {
    PanicAlertT("Failed to create D3D swap chain");
    ShutdownShared();
    D3D::Destroy();
    return false;
  }

  g_renderer = std::make_unique<Renderer>(std::move(swap_chain), wsi.render_surface_scale);
  g_vertex_manager = std::make_unique<VertexManager>();
  g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_texture_cache = std::make_unique<TextureCacheBase>();
  g_perf_query = std::make_unique<PerfQuery>();
  if (!g_renderer->Initialize() || !g_vertex_manager->Initialize() ||
      !g_shader_cache->Initialize() || !g_framebuffer_manager->Initialize() ||
      !g_texture_cache->Initialize())
  {
    Shutdown();
    return false;
  }

  BBox::Init();
  g_shader_cache->InitializeShaderCache();
  return true;
}

void VideoBackend::Shutdown()
{
  g_shader_cache->Shutdown();
  g_renderer->Shutdown();

  BBox::Shutdown();

  g_perf_query.reset();
  g_texture_cache.reset();
  g_framebuffer_manager.reset();
  g_shader_cache.reset();
  g_vertex_manager.reset();
  g_renderer.reset();

  ShutdownShared();
  D3D::Destroy();
}
}  // namespace DX11
