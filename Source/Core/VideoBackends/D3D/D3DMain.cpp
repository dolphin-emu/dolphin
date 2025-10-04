// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/VideoBackend.h"

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBoundingBox.h"
#include "VideoBackends/D3D/D3DGfx.h"
#include "VideoBackends/D3D/D3DPerfQuery.h"
#include "VideoBackends/D3D/D3DSwapChain.h"
#include "VideoBackends/D3D/D3DVertexManager.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
std::string VideoBackend::GetName() const
{
  return NAME;
}

std::string VideoBackend::GetDisplayName() const
{
  return _trans("Direct3D 11");
}

std::optional<std::string> VideoBackend::GetWarningMessage() const
{
  std::optional<std::string> result;

  // If relevant, show a warning about partial DX11.1 support
  // This is being called BEFORE FillBackendInfo is called for this backend,
  // so query for logic op support manually
  bool supportsLogicOp = false;
  if (D3DCommon::LoadLibraries())
  {
    supportsLogicOp = D3D::SupportsLogicOp(g_Config.iAdapter);
    D3DCommon::UnloadLibraries();
  }

  if (!supportsLogicOp)
  {
    result = _trans("The Direct3D 11 renderer requires support for features not supported by your "
                    "system configuration. You may still use this backend, but you will encounter "
                    "graphical artifacts in certain games.\n"
                    "\n"
                    "Do you really want to switch to Direct3D 11? If unsure, select 'No'.");
  }

  return result;
}

void VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
{
  if (!D3DCommon::LoadLibraries())
    return;

  FillBackendInfo();
  D3DCommon::UnloadLibraries();
}

void VideoBackend::FillBackendInfo()
{
  g_backend_info.api_type = APIType::D3D;
  g_backend_info.MaxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  g_backend_info.bUsesLowerLeftOrigin = false;
  g_backend_info.bSupportsExclusiveFullscreen = true;
  g_backend_info.bSupportsDualSourceBlend = true;
  g_backend_info.bSupportsPrimitiveRestart = true;
  g_backend_info.bSupportsGeometryShaders = true;
  g_backend_info.bSupportsComputeShaders = false;
  g_backend_info.bSupports3DVision = true;
  g_backend_info.bSupportsPostProcessing = true;
  g_backend_info.bSupportsPaletteConversion = true;
  g_backend_info.bSupportsClipControl = true;
  g_backend_info.bSupportsDepthClamp = true;
  g_backend_info.bSupportsReversedDepthRange = false;
  g_backend_info.bSupportsMultithreading = false;
  g_backend_info.bSupportsGPUTextureDecoding = true;
  g_backend_info.bSupportsCopyToVram = true;
  g_backend_info.bSupportsLargePoints = false;
  g_backend_info.bSupportsDepthReadback = true;
  g_backend_info.bSupportsPartialDepthCopies = false;
  g_backend_info.bSupportsBitfield = false;
  g_backend_info.bSupportsDynamicSamplerIndexing = false;
  g_backend_info.bSupportsFramebufferFetch = false;
  g_backend_info.bSupportsBackgroundCompiling = true;
  g_backend_info.bSupportsST3CTextures = true;
  g_backend_info.bSupportsBPTCTextures = true;
  g_backend_info.bSupportsEarlyZ = true;
  g_backend_info.bSupportsBBox = true;
  g_backend_info.bSupportsFragmentStoresAndAtomics = true;
  g_backend_info.bSupportsGSInstancing = true;
  g_backend_info.bSupportsSSAA = true;
  g_backend_info.bSupportsShaderBinaries = true;
  g_backend_info.bSupportsPipelineCacheData = false;
  g_backend_info.bSupportsCoarseDerivatives = true;
  g_backend_info.bSupportsTextureQueryLevels = true;
  g_backend_info.bSupportsLodBiasInSampler = true;
  g_backend_info.bSupportsLogicOp = D3D::SupportsLogicOp(g_Config.iAdapter);
  g_backend_info.bSupportsSettingObjectNames = true;
  g_backend_info.bSupportsPartialMultisampleResolve = true;
  g_backend_info.bSupportsDynamicVertexLoader = false;
  g_backend_info.bSupportsHDROutput = true;

  g_backend_info.Adapters = D3DCommon::GetAdapterNames();
  g_backend_info.AAModes = D3D::GetAAModes(g_Config.iAdapter);

  // Override optional features if we are actually booting.
  if (D3D::device)
  {
    g_backend_info.bSupportsST3CTextures = D3D::SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
                                           D3D::SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
                                           D3D::SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
    g_backend_info.bSupportsBPTCTextures = D3D::SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);

    // Features only supported with a FL11.0+ device.
    const bool shader_model_5_supported = D3D::feature_level >= D3D_FEATURE_LEVEL_11_0;
    g_backend_info.bSupportsEarlyZ = shader_model_5_supported;
    g_backend_info.bSupportsBBox = shader_model_5_supported;
    g_backend_info.bSupportsFragmentStoresAndAtomics = shader_model_5_supported;
    g_backend_info.bSupportsGSInstancing = shader_model_5_supported;
    g_backend_info.bSupportsSSAA = shader_model_5_supported;
    g_backend_info.bSupportsGPUTextureDecoding = shader_model_5_supported;
  }
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  if (!D3D::Create(g_Config.iAdapter, g_Config.bEnableValidationLayer))
    return false;

  FillBackendInfo();
  UpdateActiveConfig();

  std::unique_ptr<SwapChain> swap_chain;
  if (wsi.render_surface && !(swap_chain = SwapChain::Create(wsi)))
  {
    PanicAlertFmtT("Failed to create D3D swap chain");
    ShutdownShared();
    D3D::Destroy();
    return false;
  }

  auto gfx = std::make_unique<DX11::Gfx>(std::move(swap_chain), wsi.render_surface_scale);
  auto vertex_manager = std::make_unique<VertexManager>();
  auto perf_query = std::make_unique<PerfQuery>();
  auto bounding_box = std::make_unique<D3DBoundingBox>();

  return InitializeShared(
      std::move(gfx), std::move(vertex_manager), std::move(perf_query), std::move(bounding_box));
}

void VideoBackend::Shutdown()
{
  ShutdownShared();
  D3D::Destroy();
}
}  // namespace DX11
