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

  // If user is on Win7, show a warning about partial DX11.1 support
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
    result = _trans("Direct3D 11 renderer requires support for features not supported by your "
                    "system configuration. This is most likely because you are using Windows 7. "
                    "You may still use this backend, but you might encounter graphical artifacts."
                    "\n\nDo you really want to switch to Direct3D 11? If unsure, select 'No'.");
  }

  return result;
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
  backend_info.api_type = APIType::D3D;
  backend_info.MaxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  backend_info.bUsesLowerLeftOrigin = false;
  backend_info.bSupportsExclusiveFullscreen = true;
  backend_info.bSupportsDualSourceBlend = true;
  backend_info.bSupportsPrimitiveRestart = true;
  backend_info.bSupportsGeometryShaders = true;
  backend_info.bSupportsComputeShaders = false;
  backend_info.bSupports3DVision = true;
  backend_info.bSupportsPostProcessing = true;
  backend_info.bSupportsPaletteConversion = true;
  backend_info.bSupportsClipControl = true;
  backend_info.bSupportsDepthClamp = true;
  backend_info.bSupportsReversedDepthRange = false;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsGPUTextureDecoding = true;
  backend_info.bSupportsCopyToVram = true;
  backend_info.bSupportsLargePoints = false;
  backend_info.bSupportsDepthReadback = true;
  backend_info.bSupportsPartialDepthCopies = false;
  backend_info.bSupportsBitfield = false;
  backend_info.bSupportsDynamicSamplerIndexing = false;
  backend_info.bSupportsFramebufferFetch = false;
  backend_info.bSupportsBackgroundCompiling = true;
  backend_info.bSupportsST3CTextures = true;
  backend_info.bSupportsBPTCTextures = true;
  backend_info.bSupportsEarlyZ = true;
  backend_info.bSupportsBBox = true;
  backend_info.bSupportsFragmentStoresAndAtomics = true;
  backend_info.bSupportsGSInstancing = true;
  backend_info.bSupportsSSAA = true;
  backend_info.bSupportsShaderBinaries = true;
  backend_info.bSupportsPipelineCacheData = false;
  backend_info.bSupportsCoarseDerivatives = true;
  backend_info.bSupportsTextureQueryLevels = true;
  backend_info.bSupportsLodBiasInSampler = true;
  backend_info.bSupportsLogicOp = D3D::SupportsLogicOp(g_Config.iAdapter);
  backend_info.bSupportsSettingObjectNames = true;
  backend_info.bSupportsPartialMultisampleResolve = true;
  backend_info.bSupportsDynamicVertexLoader = false;

  backend_info.Adapters = D3DCommon::GetAdapterNames();
  backend_info.AAModes = D3D::GetAAModes(g_Config.iAdapter);

  // Override optional features if we are actually booting.
  if (D3D::device)
  {
    backend_info.bSupportsST3CTextures = D3D::SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
                                         D3D::SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
                                         D3D::SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
    backend_info.bSupportsBPTCTextures = D3D::SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);

    // Features only supported with a FL11.0+ device.
    const bool shader_model_5_supported = D3D::feature_level >= D3D_FEATURE_LEVEL_11_0;
    backend_info.bSupportsEarlyZ = shader_model_5_supported;
    backend_info.bSupportsBBox = shader_model_5_supported;
    backend_info.bSupportsFragmentStoresAndAtomics = shader_model_5_supported;
    backend_info.bSupportsGSInstancing = shader_model_5_supported;
    backend_info.bSupportsSSAA = shader_model_5_supported;
    backend_info.bSupportsGPUTextureDecoding = shader_model_5_supported;
  }
}

std::unique_ptr<AbstractGfx> VideoBackend::CreateGfx()
{
  if (!D3D::Create(g_Config.iAdapter, g_Config.bEnableValidationLayer))
    return {};

  FillBackendInfo();
  UpdateActiveConfig();

  std::unique_ptr<SwapChain> swap_chain;
  if (m_wsi.render_surface && !(swap_chain = SwapChain::Create(m_wsi, backend_info)))
  {
    PanicAlertFmtT("Failed to create D3D swap chain");
    D3D::Destroy();
    return {};
  }

  return std::make_unique<DX11::Gfx>(this, std::move(swap_chain), m_wsi.render_surface_scale);
}

std::unique_ptr<VertexManagerBase> VideoBackend::CreateVertexManager()
{
  return std::make_unique<DX11::VertexManager>();
}

std::unique_ptr<PerfQueryBase> VideoBackend::CreatePerfQuery()
{
  return std::make_unique<DX11::PerfQuery>();
}

std::unique_ptr<BoundingBox> VideoBackend::CreateBoundingBox()
{
  return std::make_unique<DX11::D3DBoundingBox>();
}

void VideoBackend::Shutdown()
{
  D3D::Destroy();
}
}  // namespace DX11
