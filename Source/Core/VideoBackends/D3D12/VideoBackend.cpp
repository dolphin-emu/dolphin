// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/VideoBackend.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12BoundingBox.h"
#include "VideoBackends/D3D12/D3D12Gfx.h"
#include "VideoBackends/D3D12/D3D12PerfQuery.h"
#include "VideoBackends/D3D12/D3D12SwapChain.h"
#include "VideoBackends/D3D12/D3D12VertexManager.h"
#include "VideoBackends/D3D12/DX12Context.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
std::string VideoBackend::GetName() const
{
  return NAME;
}

std::string VideoBackend::GetDisplayName() const
{
  return "Direct3D 12";
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
  backend_info.bUsesLowerLeftOrigin = false;
  backend_info.bSupportsExclusiveFullscreen = true;
  backend_info.bSupportsDualSourceBlend = true;
  backend_info.bSupportsPrimitiveRestart = true;
  backend_info.bSupportsGeometryShaders = true;
  backend_info.bSupports3DVision = false;
  backend_info.bSupportsEarlyZ = true;
  backend_info.bSupportsBindingLayout = false;
  backend_info.bSupportsBBox = true;
  backend_info.bSupportsGSInstancing = true;
  backend_info.bSupportsPaletteConversion = true;
  backend_info.bSupportsPostProcessing = true;
  backend_info.bSupportsClipControl = true;
  backend_info.bSupportsSSAA = true;
  backend_info.bSupportsFragmentStoresAndAtomics = true;
  backend_info.bSupportsDepthClamp = true;
  backend_info.bSupportsReversedDepthRange = false;
  backend_info.bSupportsComputeShaders = true;
  backend_info.bSupportsLogicOp = true;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsGPUTextureDecoding = true;
  backend_info.bSupportsST3CTextures = false;
  backend_info.bSupportsCopyToVram = true;
  backend_info.bSupportsBitfield = false;
  backend_info.bSupportsDynamicSamplerIndexing = false;
  backend_info.bSupportsBPTCTextures = false;
  backend_info.bSupportsFramebufferFetch = false;
  backend_info.bSupportsBackgroundCompiling = true;
  backend_info.bSupportsLargePoints = false;
  backend_info.bSupportsDepthReadback = true;
  backend_info.bSupportsPartialDepthCopies = false;
  backend_info.Adapters = D3DCommon::GetAdapterNames();
  backend_info.AAModes = DXContext::GetAAModes(g_Config.iAdapter);
  backend_info.bSupportsShaderBinaries = true;
  backend_info.bSupportsPipelineCacheData = true;
  backend_info.bSupportsCoarseDerivatives = true;
  backend_info.bSupportsTextureQueryLevels = true;
  backend_info.bSupportsLodBiasInSampler = true;
  backend_info.bSupportsSettingObjectNames = true;
  backend_info.bSupportsPartialMultisampleResolve = true;
  backend_info.bSupportsDynamicVertexLoader = true;
  backend_info.bSupportsVSLinePointExpand = true;

  // We can only check texture support once we have a device.
  if (g_dx_context)
  {
    backend_info.bSupportsST3CTextures =
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
    backend_info.bSupportsBPTCTextures = g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);
  }
}

std::unique_ptr<AbstractGfx> VideoBackend::CreateGfx()
{
  if (!DXContext::Create(g_Config.iAdapter, g_Config.bEnableValidationLayer))
  {
    PanicAlertFmtT("Failed to create D3D12 context");
    return {};
  }

  FillBackendInfo();
  UpdateActiveConfig();

  if (!g_dx_context->CreateGlobalResources(backend_info))
  {
    PanicAlertFmtT("Failed to create D3D12 global resources");
    DXContext::Destroy();
    return {};
  }

  std::unique_ptr<SwapChain> swap_chain;
  if (m_wsi.render_surface && !(swap_chain = SwapChain::Create(m_wsi, backend_info)))
  {
    PanicAlertFmtT("Failed to create D3D swap chain");
    DXContext::Destroy();
    return {};
  }

  return std::make_unique<DX12::Gfx>(this, std::move(swap_chain), m_wsi.render_surface_scale);
}

std::unique_ptr<VertexManagerBase> VideoBackend::CreateVertexManager()
{
  return std::make_unique<DX12::VertexManager>();
}

std::unique_ptr<PerfQueryBase> VideoBackend::CreatePerfQuery()
{
  return std::make_unique<DX12::PerfQuery>();
}

std::unique_ptr<BoundingBox> VideoBackend::CreateBoundingBox()
{
  return std::make_unique<DX12::D3D12BoundingBox>();
}

void VideoBackend::Shutdown()
{
  DXContext::Destroy();
}
}  // namespace DX12
