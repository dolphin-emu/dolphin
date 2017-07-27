// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PerfQuery.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoBackends/D3D/VideoBackend.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
unsigned int VideoBackend::PeekMessages()
{
  MSG msg;
  while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    if (msg.message == WM_QUIT)
      return FALSE;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return TRUE;
}

std::string VideoBackend::GetName() const
{
  return "D3D";
}

std::string VideoBackend::GetDisplayName() const
{
  return "Direct3D 11";
}

void VideoBackend::InitBackendInfo()
{
  HRESULT hr = DX11::D3D::LoadDXGI();
  if (SUCCEEDED(hr))
    hr = DX11::D3D::LoadD3D();
  if (FAILED(hr))
  {
    DX11::D3D::UnloadDXGI();
    return;
  }

  g_Config.backend_info.api_type = APIType::D3D;
  g_Config.backend_info.MaxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  g_Config.backend_info.bSupportsExclusiveFullscreen = true;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsOversizedViewports = false;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupports3DVision = true;
  g_Config.backend_info.bSupportsPostProcessing = false;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsReversedDepthRange = false;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsInternalResolutionFrameDumps = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = false;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBitfield = false;
  g_Config.backend_info.bSupportsDynamicSamplerIndexing = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;

  IDXGIFactory* factory;
  IDXGIAdapter* ad;
  hr = DX11::PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
  if (FAILED(hr))
    PanicAlert("Failed to create IDXGIFactory object");

  // adapters
  g_Config.backend_info.Adapters.clear();
  g_Config.backend_info.AAModes.clear();
  while (factory->EnumAdapters((UINT)g_Config.backend_info.Adapters.size(), &ad) !=
         DXGI_ERROR_NOT_FOUND)
  {
    const size_t adapter_index = g_Config.backend_info.Adapters.size();

    DXGI_ADAPTER_DESC desc;
    ad->GetDesc(&desc);

    // TODO: These don't get updated on adapter change, yet
    if (adapter_index == g_Config.iAdapter)
    {
      std::string samples;
      std::vector<DXGI_SAMPLE_DESC> modes = DX11::D3D::EnumAAModes(ad);
      // First iteration will be 1. This equals no AA.
      for (unsigned int i = 0; i < modes.size(); ++i)
      {
        g_Config.backend_info.AAModes.push_back(modes[i].Count);
      }

      D3D_FEATURE_LEVEL feature_level = D3D::GetFeatureLevel(ad);
      bool shader_model_5_supported = feature_level >= D3D_FEATURE_LEVEL_11_0;
      g_Config.backend_info.MaxTextureSize = D3D::GetMaxTextureSize(feature_level);

      // Requires the earlydepthstencil attribute (only available in shader model 5)
      g_Config.backend_info.bSupportsEarlyZ = shader_model_5_supported;

      // Requires full UAV functionality (only available in shader model 5)
      g_Config.backend_info.bSupportsBBox =
          g_Config.backend_info.bSupportsFragmentStoresAndAtomics = shader_model_5_supported;

      // Requires the instance attribute (only available in shader model 5)
      g_Config.backend_info.bSupportsGSInstancing = shader_model_5_supported;

      // Sample shading requires shader model 5
      g_Config.backend_info.bSupportsSSAA = shader_model_5_supported;
    }
    g_Config.backend_info.Adapters.push_back(UTF16ToUTF8(desc.Description));
    ad->Release();
  }
  factory->Release();

  DX11::D3D::UnloadDXGI();
  DX11::D3D::UnloadD3D();
}

bool VideoBackend::Initialize(void* window_handle)
{
  if (window_handle == nullptr)
    return false;

  InitBackendInfo();
  InitializeShared();

  m_window_handle = window_handle;

  return true;
}

void VideoBackend::Video_Prepare()
{
  if (FAILED(D3D::Create(reinterpret_cast<HWND>(m_window_handle))))
    PanicAlert("Failed to create D3D device.");

  // internal interfaces
  g_renderer = std::make_unique<Renderer>();
  g_texture_cache = std::make_unique<TextureCache>();
  g_vertex_manager = std::make_unique<VertexManager>();
  g_perf_query = std::make_unique<PerfQuery>();
  VertexShaderCache::Init();
  PixelShaderCache::Init();
  GeometryShaderCache::Init();
  VertexShaderCache::WaitForBackgroundCompilesToComplete();
  D3D::InitUtils();
  BBox::Init();
}

void VideoBackend::Shutdown()
{
  // TODO: should be in Video_Cleanup
  D3D::ShutdownUtils();
  PixelShaderCache::Shutdown();
  VertexShaderCache::Shutdown();
  GeometryShaderCache::Shutdown();
  BBox::Shutdown();

  g_perf_query.reset();
  g_vertex_manager.reset();
  g_texture_cache.reset();
  g_renderer.reset();

  ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
  CleanupShared();
}
}
