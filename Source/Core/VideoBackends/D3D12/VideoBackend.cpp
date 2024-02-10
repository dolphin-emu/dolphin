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

void VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
{
  if (!D3DCommon::LoadLibraries())
    return;

  FillBackendInfo();
  D3DCommon::UnloadLibraries();
}

void VideoBackend::FillBackendInfo()
{
  g_Config.backend_info.api_type = APIType::D3D;
  g_Config.backend_info.bUsesLowerLeftOrigin = false;
  g_Config.backend_info.bSupportsExclusiveFullscreen = true;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsBindingLayout = false;
  g_Config.backend_info.bSupportsBBox = true;
  g_Config.backend_info.bSupportsGSInstancing = true;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsPostProcessing = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsSSAA = true;
  g_Config.backend_info.bSupportsFragmentStoresAndAtomics = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsReversedDepthRange = false;
  g_Config.backend_info.bSupportsComputeShaders = true;
  g_Config.backend_info.bSupportsLogicOp = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = true;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsCopyToVram = true;
  g_Config.backend_info.bSupportsBitfield = false;
  g_Config.backend_info.bSupportsDynamicSamplerIndexing = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;
  g_Config.backend_info.bSupportsFramebufferFetch = false;
  g_Config.backend_info.bSupportsBackgroundCompiling = true;
  g_Config.backend_info.bSupportsLargePoints = false;
  g_Config.backend_info.bSupportsDepthReadback = true;
  g_Config.backend_info.bSupportsPartialDepthCopies = false;
  g_Config.backend_info.Adapters = D3DCommon::GetAdapterNames();
  g_Config.backend_info.AAModes = DXContext::GetAAModes(g_Config.iAdapter);
  g_Config.backend_info.bSupportsShaderBinaries = true;
  g_Config.backend_info.bSupportsPipelineCacheData = true;
  g_Config.backend_info.bSupportsCoarseDerivatives = true;
  g_Config.backend_info.bSupportsTextureQueryLevels = true;
  g_Config.backend_info.bSupportsLodBiasInSampler = true;
  g_Config.backend_info.bSupportsSettingObjectNames = true;
  g_Config.backend_info.bSupportsPartialMultisampleResolve = true;
  g_Config.backend_info.bSupportsDynamicVertexLoader = true;
  g_Config.backend_info.bSupportsVSLinePointExpand = true;
  g_Config.backend_info.bSupportsHDROutput = true;

  // We can only check texture support once we have a device.
  if (g_dx_context)
  {
    g_Config.backend_info.bSupportsST3CTextures =
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
    g_Config.backend_info.bSupportsBPTCTextures =
        g_dx_context->SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);
  }
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  if (!DXContext::Create(g_Config.iAdapter, g_Config.bEnableValidationLayer))
  {
    PanicAlertFmtT("Failed to create D3D12 context");
    return false;
  }

  FillBackendInfo();
  UpdateActiveConfig();

  if (!g_dx_context->CreateGlobalResources())
  {
    PanicAlertFmtT("Failed to create D3D12 global resources");
    DXContext::Destroy();
    ShutdownShared();
    return false;
  }

  std::unique_ptr<SwapChain> swap_chain;
  if (wsi.render_surface && !(swap_chain = SwapChain::Create(wsi)))
  {
    PanicAlertFmtT("Failed to create D3D swap chain");
    DXContext::Destroy();
    ShutdownShared();
    return false;
  }

  // Create main wrapper instances.
  auto gfx = std::make_unique<DX12::Gfx>(std::move(swap_chain), wsi.render_surface_scale);
  auto vertex_manager = std::make_unique<DX12::VertexManager>();
  auto perf_query = std::make_unique<DX12::PerfQuery>();
  auto bounding_box = std::make_unique<DX12::D3D12BoundingBox>();

  return InitializeShared(std::move(gfx), std::move(vertex_manager), std::move(perf_query),
                          std::move(bounding_box));
}

void VideoBackend::Shutdown()
{
  // Keep the debug runtime happy...
  if (g_gfx)
    Gfx::GetInstance()->ExecuteCommandList(true);

  ShutdownShared();
  DXContext::Destroy();
}
}  // namespace DX12
