// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
using D3DREFLECT = HRESULT(WINAPI*)(LPCVOID, SIZE_T, REFIID, void**);

static HINSTANCE s_d3d_compiler_dll;
static int s_d3dcompiler_dll_ref;
static D3DREFLECT s_d3d_reflect;
pD3DCompile PD3DCompile = nullptr;

CREATEDXGIFACTORY PCreateDXGIFactory = nullptr;
static HINSTANCE s_dxgi_dll;
static int s_dxgi_dll_ref;

static D3D11CREATEDEVICE s_d3d11_create_device;
static HINSTANCE s_d3d_dll;
static int s_d3d_dll_ref;

namespace D3D
{
ID3D11Device* device = nullptr;
ID3D11Device1* device1 = nullptr;
ID3D11DeviceContext* context = nullptr;
IDXGISwapChain1* swapchain = nullptr;

static IDXGIFactory2* s_dxgi_factory;
static ID3D11Debug* s_debug;
static D3D_FEATURE_LEVEL s_featlevel;
static D3DTexture2D* s_backbuf;

static std::vector<DXGI_SAMPLE_DESC> s_aa_modes;  // supported AA modes of the current adapter

static bool s_bgra_textures_supported;
static bool s_allow_tearing_supported;

constexpr UINT NUM_SUPPORTED_FEATURE_LEVELS = 3;
constexpr D3D_FEATURE_LEVEL supported_feature_levels[NUM_SUPPORTED_FEATURE_LEVELS] = {
    D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

HRESULT LoadDXGI()
{
  if (s_dxgi_dll_ref++ > 0)
    return S_OK;

  if (s_dxgi_dll)
    return S_OK;
  s_dxgi_dll = LoadLibraryA("dxgi.dll");
  if (!s_dxgi_dll)
  {
    MessageBoxA(nullptr, "Failed to load dxgi.dll", "Critical error", MB_OK | MB_ICONERROR);
    --s_dxgi_dll_ref;
    return E_FAIL;
  }

  // Even though we use IDXGIFactory2 we use CreateDXGIFactory1 to create it to maintain
  // compatibility with Windows 7
  PCreateDXGIFactory = (CREATEDXGIFACTORY)GetProcAddress(s_dxgi_dll, "CreateDXGIFactory1");
  if (PCreateDXGIFactory == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for CreateDXGIFactory1!", "Critical error",
                MB_OK | MB_ICONERROR);

  return S_OK;
}

HRESULT LoadD3D()
{
  if (s_d3d_dll_ref++ > 0)
    return S_OK;

  if (s_d3d_dll)
    return S_OK;
  s_d3d_dll = LoadLibraryA("d3d11.dll");
  if (!s_d3d_dll)
  {
    MessageBoxA(nullptr, "Failed to load d3d11.dll", "Critical error", MB_OK | MB_ICONERROR);
    --s_d3d_dll_ref;
    return E_FAIL;
  }
  s_d3d11_create_device = (D3D11CREATEDEVICE)GetProcAddress(s_d3d_dll, "D3D11CreateDevice");
  if (s_d3d11_create_device == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3D11CreateDevice!", "Critical error",
                MB_OK | MB_ICONERROR);

  return S_OK;
}

HRESULT LoadD3DCompiler()
{
  if (s_d3dcompiler_dll_ref++ > 0)
    return S_OK;
  if (s_d3d_compiler_dll)
    return S_OK;

  // The older version of the D3D compiler cannot compile our ubershaders without various
  // graphical issues. D3DCOMPILER_DLL_A should point to d3dcompiler_47.dll, so if this fails
  // to load, inform the user that they need to update their system.
  s_d3d_compiler_dll = LoadLibraryA(D3DCOMPILER_DLL_A);
  if (!s_d3d_compiler_dll)
  {
    PanicAlertT("Failed to load %s. If you are using Windows 7, try installing the "
                "KB4019990 update package.",
                D3DCOMPILER_DLL_A);
    return E_FAIL;
  }

  s_d3d_reflect = (D3DREFLECT)GetProcAddress(s_d3d_compiler_dll, "D3DReflect");
  if (s_d3d_reflect == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3DReflect!", "Critical error",
                MB_OK | MB_ICONERROR);
  PD3DCompile = (pD3DCompile)GetProcAddress(s_d3d_compiler_dll, "D3DCompile");
  if (PD3DCompile == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3DCompile!", "Critical error",
                MB_OK | MB_ICONERROR);

  return S_OK;
}

void UnloadDXGI()
{
  if (!s_dxgi_dll_ref)
    return;
  if (--s_dxgi_dll_ref != 0)
    return;

  if (s_dxgi_dll)
    FreeLibrary(s_dxgi_dll);
  s_dxgi_dll = nullptr;
  PCreateDXGIFactory = nullptr;
}

void UnloadD3D()
{
  if (!s_d3d_dll_ref)
    return;
  if (--s_d3d_dll_ref != 0)
    return;

  if (s_d3d_dll)
    FreeLibrary(s_d3d_dll);
  s_d3d_dll = nullptr;
  s_d3d11_create_device = nullptr;
}

void UnloadD3DCompiler()
{
  if (!s_d3dcompiler_dll_ref)
    return;
  if (--s_d3dcompiler_dll_ref != 0)
    return;

  if (s_d3d_compiler_dll)
    FreeLibrary(s_d3d_compiler_dll);
  s_d3d_compiler_dll = nullptr;
  s_d3d_reflect = nullptr;
}

std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter)
{
  std::vector<DXGI_SAMPLE_DESC> _aa_modes;

  // NOTE: D3D 10.0 doesn't support multisampled resources which are bound as depth buffers AND
  // shader resources.
  // Thus, we can't have MSAA with 10.0 level hardware.
  ID3D11Device* _device;
  ID3D11DeviceContext* _context;
  D3D_FEATURE_LEVEL feat_level;
  HRESULT hr = s_d3d11_create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                     supported_feature_levels, NUM_SUPPORTED_FEATURE_LEVELS,
                                     D3D11_SDK_VERSION, &_device, &feat_level, &_context);
  if (FAILED(hr) || feat_level == D3D_FEATURE_LEVEL_10_0)
  {
    DXGI_SAMPLE_DESC desc;
    desc.Count = 1;
    desc.Quality = 0;
    _aa_modes.push_back(desc);
    SAFE_RELEASE(_context);
    SAFE_RELEASE(_device);
  }
  else
  {
    for (int samples = 0; samples < D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; ++samples)
    {
      UINT quality_levels = 0;
      _device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, samples, &quality_levels);

      DXGI_SAMPLE_DESC desc;
      desc.Count = samples;
      desc.Quality = 0;

      if (quality_levels > 0)
        _aa_modes.push_back(desc);
    }
    _context->Release();
    _device->Release();
  }
  return _aa_modes;
}

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter)
{
  D3D_FEATURE_LEVEL feat_level = D3D_FEATURE_LEVEL_9_1;
  s_d3d11_create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, supported_feature_levels,
                        NUM_SUPPORTED_FEATURE_LEVELS, D3D11_SDK_VERSION, nullptr, &feat_level,
                        nullptr);
  return feat_level;
}

static bool SupportsS3TCTextures(ID3D11Device* dev)
{
  UINT bc1_support, bc2_support, bc3_support;
  if (FAILED(dev->CheckFormatSupport(DXGI_FORMAT_BC1_UNORM, &bc1_support)) ||
      FAILED(dev->CheckFormatSupport(DXGI_FORMAT_BC2_UNORM, &bc2_support)) ||
      FAILED(dev->CheckFormatSupport(DXGI_FORMAT_BC3_UNORM, &bc3_support)))
  {
    return false;
  }

  return ((bc1_support & bc2_support & bc3_support) & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
}

static bool SupportsBPTCTextures(ID3D11Device* dev)
{
  // Currently, we only care about BC7. This could be extended to BC6H in the future.
  UINT bc7_support;
  if (FAILED(dev->CheckFormatSupport(DXGI_FORMAT_BC7_UNORM, &bc7_support)))
    return false;

  return (bc7_support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
}

static bool CreateSwapChainTextures()
{
  ID3D11Texture2D* buf;
  HRESULT hr = swapchain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&buf);
  CHECK(SUCCEEDED(hr), "GetBuffer for swap chain failed with HRESULT %08X", hr);
  if (FAILED(hr))
    return false;

  s_backbuf = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
  SAFE_RELEASE(buf);
  SetDebugObjectName(s_backbuf->GetTex(), "backbuffer texture");
  SetDebugObjectName(s_backbuf->GetRTV(), "backbuffer render target view");
  return true;
}

static bool CreateSwapChain(HWND hWnd)
{
  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
  swap_chain_desc.BufferCount = 2;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.SampleDesc.Quality = 0;
  swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swap_chain_desc.Stereo = g_ActiveConfig.stereo_mode == StereoMode::QuadBuffer;

  // This flag is necessary if we want to use a flip-model swapchain without locking the framerate
  swap_chain_desc.Flags = s_allow_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  HRESULT hr = s_dxgi_factory->CreateSwapChainForHwnd(device, hWnd, &swap_chain_desc, nullptr,
                                                      nullptr, &swapchain);
  if (FAILED(hr))
  {
    // Flip-model discard swapchains aren't supported on Windows 8, so here we fall back to
    // a sequential swapchain
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    hr = s_dxgi_factory->CreateSwapChainForHwnd(device, hWnd, &swap_chain_desc, nullptr, nullptr,
                                                &swapchain);
  }

  if (FAILED(hr))
  {
    // Flip-model swapchains aren't supported on Windows 7, so here we fall back to a legacy
    // BitBlt-model swapchain
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    hr = s_dxgi_factory->CreateSwapChainForHwnd(device, hWnd, &swap_chain_desc, nullptr, nullptr,
                                                &swapchain);
  }

  if (FAILED(hr))
  {
    ERROR_LOG(VIDEO, "Failed to create swap chain with HRESULT %08X", hr);
    return false;
  }

  if (!CreateSwapChainTextures())
  {
    SAFE_RELEASE(swapchain);
    return false;
  }

  return true;
}

HRESULT Create(HWND wnd)
{
  HRESULT hr = LoadDXGI();
  if (SUCCEEDED(hr))
    hr = LoadD3D();
  if (SUCCEEDED(hr))
    hr = LoadD3DCompiler();
  if (FAILED(hr))
  {
    UnloadDXGI();
    UnloadD3D();
    UnloadD3DCompiler();
    return hr;
  }

  hr = PCreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)&s_dxgi_factory);
  if (FAILED(hr))
    MessageBox(wnd, _T("Failed to create IDXGIFactory object"), _T("Dolphin Direct3D 11 backend"),
               MB_OK | MB_ICONERROR);

  IDXGIAdapter* adapter;
  hr = s_dxgi_factory->EnumAdapters(g_ActiveConfig.iAdapter, &adapter);
  if (FAILED(hr))
  {
    // try using the first one
    hr = s_dxgi_factory->EnumAdapters(0, &adapter);
    if (FAILED(hr))
      MessageBox(wnd, _T("Failed to enumerate adapters"), _T("Dolphin Direct3D 11 backend"),
                 MB_OK | MB_ICONERROR);
  }

  // get supported AA modes
  s_aa_modes = EnumAAModes(adapter);

  if (std::find_if(s_aa_modes.begin(), s_aa_modes.end(), [](const DXGI_SAMPLE_DESC& desc) {
        return desc.Count == g_Config.iMultisamples;
      }) == s_aa_modes.end())
  {
    Config::SetCurrent(Config::GFX_MSAA, UINT32_C(1));
    UpdateActiveConfig();
  }

  // Check support for allow tearing, we query the interface for backwards compatibility
  UINT allow_tearing = FALSE;
  IDXGIFactory5* factory5;
  hr = s_dxgi_factory->QueryInterface(&factory5);
  if (SUCCEEDED(hr))
  {
    hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing,
                                       sizeof(allow_tearing));
    factory5->Release();
  }
  s_allow_tearing_supported = SUCCEEDED(hr) && allow_tearing;

  // Creating debug devices can sometimes fail if the user doesn't have the correct
  // version of the DirectX SDK. If it does, simply fallback to a non-debug device.
  if (g_Config.bEnableValidationLayer)
  {
    hr = s_d3d11_create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_DEBUG,
                               supported_feature_levels, NUM_SUPPORTED_FEATURE_LEVELS,
                               D3D11_SDK_VERSION, &device, &s_featlevel, &context);

    // Debugbreak on D3D error
    if (SUCCEEDED(hr) && SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Debug), (void**)&s_debug)))
    {
      ID3D11InfoQueue* infoQueue = nullptr;
      if (SUCCEEDED(s_debug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&infoQueue)))
      {
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

        D3D11_MESSAGE_ID hide[] = {D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS};

        D3D11_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = sizeof(hide) / sizeof(D3D11_MESSAGE_ID);
        filter.DenyList.pIDList = hide;
        infoQueue->AddStorageFilterEntries(&filter);
        infoQueue->Release();
      }
    }
  }

  if (!g_Config.bEnableValidationLayer || FAILED(hr))
  {
    hr = s_d3d11_create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                               supported_feature_levels, NUM_SUPPORTED_FEATURE_LEVELS,
                               D3D11_SDK_VERSION, &device, &s_featlevel, &context);
  }

  SAFE_RELEASE(adapter);

  if (FAILED(hr) || (wnd && !CreateSwapChain(wnd)))
  {
    MessageBox(
        wnd,
        _T("Failed to initialize Direct3D.\nMake sure your video card supports at least D3D 10.0"),
        _T("Dolphin Direct3D 11 backend"), MB_OK | MB_ICONERROR);
    SAFE_RELEASE(device);
    SAFE_RELEASE(context);
    SAFE_RELEASE(s_dxgi_factory);
    return E_FAIL;
  }

  hr = device->QueryInterface<ID3D11Device1>(&device1);
  if (FAILED(hr))
  {
    WARN_LOG(VIDEO, "Missing Direct3D 11.1 support. Logical operations will not be supported.");
    g_Config.backend_info.bSupportsLogicOp = false;
  }

  // BGRA textures are easier to deal with in TextureCache, but might not be supported
  UINT format_support;
  device->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM, &format_support);
  s_bgra_textures_supported = (format_support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
  g_Config.backend_info.bSupportsST3CTextures = SupportsS3TCTextures(device);
  g_Config.backend_info.bSupportsBPTCTextures = SupportsBPTCTextures(device);

  // prevent DXGI from responding to Alt+Enter, unfortunately DXGI_MWA_NO_ALT_ENTER
  // does not work so we disable all monitoring of window messages. However this
  // may make it more difficult for DXGI to handle display mode changes.
  if (wnd)
  {
    hr = s_dxgi_factory->MakeWindowAssociation(wnd, DXGI_MWA_NO_WINDOW_CHANGES);
    if (FAILED(hr))
      MessageBox(wnd, _T("Failed to associate the window"), _T("Dolphin Direct3D 11 backend"),
                 MB_OK | MB_ICONERROR);
  }

  SetDebugObjectName(context, "device context");

  stateman = new StateManager;
  return S_OK;
}

void Close()
{
  // we can't release the swapchain while in fullscreen.
  if (swapchain)
    swapchain->SetFullscreenState(false, nullptr);

  // release all bound resources
  context->ClearState();
  SAFE_RELEASE(s_backbuf);
  SAFE_RELEASE(swapchain);
  SAFE_DELETE(stateman);
  context->Flush();  // immediately destroy device objects

  SAFE_RELEASE(context);
  SAFE_RELEASE(device1);
  ULONG references = device->Release();

  if (s_debug)
  {
    --references;  // the debug interface increases the refcount of the device, subtract that.
    if (references)
    {
      // print out alive objects, but only if we actually have pending references
      // note this will also print out internal live objects to the debug console
      s_debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
    }
    SAFE_RELEASE(s_debug)
  }

  if (references)
  {
    ERROR_LOG(VIDEO, "Unreleased references: %i.", references);
  }
  else
  {
    NOTICE_LOG(VIDEO, "Successfully released all device references!");
  }
  device = nullptr;

  // unload DLLs
  UnloadD3D();
  UnloadDXGI();
}

const char* VertexShaderVersionString()
{
  if (s_featlevel == D3D_FEATURE_LEVEL_11_0)
    return "vs_5_0";
  else if (s_featlevel == D3D_FEATURE_LEVEL_10_1)
    return "vs_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "vs_4_0";
}

const char* GeometryShaderVersionString()
{
  if (s_featlevel == D3D_FEATURE_LEVEL_11_0)
    return "gs_5_0";
  else if (s_featlevel == D3D_FEATURE_LEVEL_10_1)
    return "gs_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "gs_4_0";
}

const char* PixelShaderVersionString()
{
  if (s_featlevel == D3D_FEATURE_LEVEL_11_0)
    return "ps_5_0";
  else if (s_featlevel == D3D_FEATURE_LEVEL_10_1)
    return "ps_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "ps_4_0";
}

const char* ComputeShaderVersionString()
{
  if (s_featlevel == D3D_FEATURE_LEVEL_11_0)
    return "cs_5_0";
  else if (s_featlevel == D3D_FEATURE_LEVEL_10_1)
    return "cs_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "cs_4_0";
}

D3DTexture2D* GetBackBuffer()
{
  return s_backbuf;
}
bool BGRATexturesSupported()
{
  return s_bgra_textures_supported;
}

bool AllowTearingSupported()
{
  return s_allow_tearing_supported;
}

// Returns the maximum width/height of a texture. This value only depends upon the feature level in
// DX11
u32 GetMaxTextureSize(D3D_FEATURE_LEVEL feature_level)
{
  switch (feature_level)
  {
  case D3D_FEATURE_LEVEL_11_0:
    return D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

  case D3D_FEATURE_LEVEL_10_1:
  case D3D_FEATURE_LEVEL_10_0:
    return D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;

  case D3D_FEATURE_LEVEL_9_3:
    return 4096;

  case D3D_FEATURE_LEVEL_9_2:
  case D3D_FEATURE_LEVEL_9_1:
    return 2048;

  default:
    return 0;
  }
}

void Reset(HWND new_wnd)
{
  SAFE_RELEASE(s_backbuf);

  if (swapchain)
  {
    if (GetFullscreenState())
      swapchain->SetFullscreenState(FALSE, nullptr);
    SAFE_RELEASE(swapchain);
  }

  if (new_wnd)
    CreateSwapChain(new_wnd);
}

void ResizeSwapChain()
{
  SAFE_RELEASE(s_backbuf);
  const UINT swap_chain_flags = AllowTearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
  swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, swap_chain_flags);
  if (!CreateSwapChainTextures())
  {
    PanicAlert("Failed to get swap chain textures");
    SAFE_RELEASE(swapchain);
  }
}

void Present()
{
  UINT present_flags = 0;

  // When using sync interval 0, it is recommended to always pass the tearing
  // flag when it is supported, even when presenting in windowed mode.
  // However, this flag cannot be used if the app is in fullscreen mode as a
  // result of calling SetFullscreenState.
  if (AllowTearingSupported() && !g_ActiveConfig.IsVSync() && !GetFullscreenState())
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;

  if (swapchain->IsTemporaryMonoSupported() && g_ActiveConfig.stereo_mode != StereoMode::QuadBuffer)
  {
    present_flags |= DXGI_PRESENT_STEREO_TEMPORARY_MONO;
  }

  // TODO: Is 1 the correct value for vsyncing?
  swapchain->Present(static_cast<UINT>(g_ActiveConfig.IsVSync()), present_flags);
}

HRESULT SetFullscreenState(bool enable_fullscreen)
{
  return swapchain->SetFullscreenState(enable_fullscreen, nullptr);
}

bool GetFullscreenState()
{
  BOOL state = FALSE;
  swapchain->GetFullscreenState(&state, nullptr);
  return !!state;
}

void SetDebugObjectName(ID3D11DeviceChild* resource, const char* name)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
  if (resource)
    resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)(name ? strlen(name) : 0), name);
#endif
}

std::string GetDebugObjectName(ID3D11DeviceChild* resource)
{
  std::string name;
#if defined(_DEBUG) || defined(DEBUGFAST)
  if (resource)
  {
    UINT size = 0;
    resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr);  // get required size
    name.resize(size);
    resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, const_cast<char*>(name.data()));
  }
#endif
  return name;
}

}  // namespace D3D

}  // namespace DX11
