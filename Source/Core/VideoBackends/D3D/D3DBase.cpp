// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/D3DBase.h"

#include <algorithm>
#include <array>

#include "Common/CommonTypes.h"
#include "Common/DynamicLibrary.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
static Common::DynamicLibrary s_d3d11_library;
namespace D3D
{
ComPtr<IDXGIFactory> dxgi_factory;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11Device1> device1;
ComPtr<ID3D11DeviceContext> context;
D3D_FEATURE_LEVEL feature_level;

static ComPtr<ID3D11Debug> s_debug;

constexpr std::array<D3D_FEATURE_LEVEL, 3> s_supported_feature_levels{
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
};

bool Create(u32 adapter_index, bool enable_debug_layer)
{
  PFN_D3D11_CREATE_DEVICE d3d11_create_device;
  if (!s_d3d11_library.Open("d3d11.dll") ||
      !s_d3d11_library.GetSymbol("D3D11CreateDevice", &d3d11_create_device))
  {
    PanicAlertFmtT("Failed to load d3d11.dll");
    s_d3d11_library.Close();
    return false;
  }

  if (!D3DCommon::LoadLibraries())
  {
    s_d3d11_library.Close();
    return false;
  }

  dxgi_factory = D3DCommon::CreateDXGIFactory(enable_debug_layer);
  if (!dxgi_factory)
  {
    PanicAlertFmtT("Failed to create DXGI factory");
    D3DCommon::UnloadLibraries();
    s_d3d11_library.Close();
    return false;
  }

  ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = dxgi_factory->EnumAdapters(adapter_index, adapter.GetAddressOf());
  if (FAILED(hr))
  {
    WARN_LOG_FMT(VIDEO, "Adapter {} not found, using default: {}", adapter_index, DX11HRWrap(hr));
    adapter = nullptr;
  }

  // Creating debug devices can sometimes fail if the user doesn't have the correct
  // version of the DirectX SDK. If it does, simply fallback to a non-debug device.
  if (enable_debug_layer)
  {
    hr = d3d11_create_device(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_DEBUG,
        s_supported_feature_levels.data(), static_cast<UINT>(s_supported_feature_levels.size()),
        D3D11_SDK_VERSION, device.GetAddressOf(), &feature_level, context.GetAddressOf());

    // Debugbreak on D3D error
    if (SUCCEEDED(hr) && SUCCEEDED(hr = device.As(&s_debug)))
    {
      ComPtr<ID3D11InfoQueue> info_queue;
      if (SUCCEEDED(s_debug.As(&info_queue)))
      {
        info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
        info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

        D3D11_MESSAGE_ID hide[] = {D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS};

        D3D11_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = sizeof(hide) / sizeof(D3D11_MESSAGE_ID);
        filter.DenyList.pIDList = hide;
        info_queue->AddStorageFilterEntries(&filter);
      }
    }
    else
    {
      WARN_LOG_FMT(VIDEO, "Debug layer requested but not available: {}", DX11HRWrap(hr));
    }
  }

  if (!enable_debug_layer || FAILED(hr))
  {
    hr = d3d11_create_device(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, s_supported_feature_levels.data(),
        static_cast<UINT>(s_supported_feature_levels.size()), D3D11_SDK_VERSION,
        device.GetAddressOf(), &feature_level, context.GetAddressOf());
  }

  if (FAILED(hr))
  {
    PanicAlertFmtT(
        "Failed to initialize Direct3D.\nMake sure your video card supports at least D3D 10.0\n{0}",
        DX11HRWrap(hr));
    dxgi_factory.Reset();
    D3DCommon::UnloadLibraries();
    s_d3d11_library.Close();
    return false;
  }

  hr = device.As(&device1);
  if (FAILED(hr))
  {
    WARN_LOG_FMT(VIDEO,
                 "Missing Direct3D 11.1 support. Logical operations will not be supported.\n{}",
                 DX11HRWrap(hr));
  }

  stateman = std::make_unique<StateManager>();
  return true;
}

void Destroy()
{
  stateman.reset();

  context->ClearState();
  context->Flush();

  context.Reset();
  device1.Reset();

  auto remaining_references = device.Reset();
  if (s_debug)
  {
    --remaining_references;  // the debug interface increases the refcount of the device, subtract
                             // that.
    if (remaining_references)
    {
      // print out alive objects, but only if we actually have pending references
      // note this will also print out internal live objects to the debug console
      s_debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
    }
    s_debug.Reset();
  }

  if (remaining_references)
    ERROR_LOG_FMT(VIDEO, "Unreleased references: {}.", remaining_references);
  else
    NOTICE_LOG_FMT(VIDEO, "Successfully released all device references!");

  dxgi_factory.Reset();
  D3DCommon::UnloadLibraries();
  s_d3d11_library.Close();
}

std::vector<u32> GetAAModes(u32 adapter_index)
{
  // Use temporary device if we don't have one already.
  Common::DynamicLibrary temp_lib;
  ComPtr<ID3D11Device> temp_device = device;
  D3D_FEATURE_LEVEL temp_feature_level = feature_level;
  if (!temp_device)
  {
    ComPtr<IDXGIFactory> temp_dxgi_factory = D3DCommon::CreateDXGIFactory(false);
    if (!temp_dxgi_factory)
      return {};

    ComPtr<IDXGIAdapter> adapter;
    temp_dxgi_factory->EnumAdapters(adapter_index, adapter.GetAddressOf());

    PFN_D3D11_CREATE_DEVICE d3d11_create_device;
    if (!temp_lib.Open("d3d11.dll") ||
        !temp_lib.GetSymbol("D3D11CreateDevice", &d3d11_create_device))
    {
      return {};
    }

    HRESULT hr = d3d11_create_device(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, s_supported_feature_levels.data(),
        static_cast<UINT>(s_supported_feature_levels.size()), D3D11_SDK_VERSION,
        temp_device.GetAddressOf(), &temp_feature_level, nullptr);
    if (FAILED(hr))
      return {};
  }

  // NOTE: D3D 10.0 doesn't support multisampled resources which are bound as depth buffers AND
  // shader resources. Thus, we can't have MSAA with 10.0 level hardware.
  if (temp_feature_level == D3D_FEATURE_LEVEL_10_0)
    return {};

  std::vector<u32> aa_modes;
  for (u32 samples = 1; samples <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; ++samples)
  {
    UINT quality_levels = 0;
    if (SUCCEEDED(temp_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, samples,
                                                             &quality_levels)) &&
        quality_levels > 0)
    {
      aa_modes.push_back(samples);
    }
  }

  return aa_modes;
}

bool SupportsTextureFormat(DXGI_FORMAT format)
{
  UINT support;
  if (FAILED(device->CheckFormatSupport(format, &support)))
    return false;

  return (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
}

bool SupportsLogicOp(u32 adapter_index)
{
  // Use temporary device if we don't have one already.
  Common::DynamicLibrary temp_lib;
  ComPtr<ID3D11Device1> temp_device1 = device1;
  if (!device)
  {
    ComPtr<ID3D11Device> temp_device;

    ComPtr<IDXGIFactory> temp_dxgi_factory = D3DCommon::CreateDXGIFactory(false);
    if (!temp_dxgi_factory)
      return false;

    ComPtr<IDXGIAdapter> adapter;
    temp_dxgi_factory->EnumAdapters(adapter_index, adapter.GetAddressOf());

    PFN_D3D11_CREATE_DEVICE d3d11_create_device;
    if (!temp_lib.Open("d3d11.dll") ||
        !temp_lib.GetSymbol("D3D11CreateDevice", &d3d11_create_device))
    {
      return false;
    }

    HRESULT hr = d3d11_create_device(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, s_supported_feature_levels.data(),
        static_cast<UINT>(s_supported_feature_levels.size()), D3D11_SDK_VERSION,
        temp_device.GetAddressOf(), nullptr, nullptr);
    if (FAILED(hr))
      return false;

    if (FAILED(temp_device.As(&temp_device1)))
      return false;
  }

  if (!temp_device1)
    return false;

  D3D11_FEATURE_DATA_D3D11_OPTIONS options{};
  if (FAILED(temp_device1->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options,
                                               sizeof(options))))
  {
    return false;
  }

  return options.OutputMergerLogicOp != FALSE;
}

}  // namespace D3D

}  // namespace DX11
