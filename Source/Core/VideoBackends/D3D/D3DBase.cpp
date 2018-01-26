// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Carl: TODO: Actually merge the QUAD BUFFERED 3D mode in D3D from "Merge pull request #5697 from Armada651/quad-buffer"

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
#include "VideoCommon/VR.h"
#include "VideoCommon/VideoConfig.h"

extern LUID* g_hmd_luid;

namespace DX11
{
HINSTANCE hD3DCompilerDll = nullptr;
D3DREFLECT PD3DReflect = nullptr;
pD3DCompile PD3DCompile = nullptr;
int d3dcompiler_dll_ref = 0;

CREATEDXGIFACTORY PCreateDXGIFactory = nullptr, PCreateDXGIFactory1 = nullptr;
HINSTANCE hDXGIDll = nullptr;
int dxgi_dll_ref = 0;

typedef HRESULT(WINAPI* D3D11CREATEDEVICEANDSWAPCHAIN)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE,
                                                       UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT,
                                                       CONST DXGI_SWAP_CHAIN_DESC*,
                                                       IDXGISwapChain**, ID3D11Device**,
                                                       D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
static D3D11CREATEDEVICE PD3D11CreateDevice = nullptr;
D3D11CREATEDEVICEANDSWAPCHAIN PD3D11CreateDeviceAndSwapChain = nullptr;
HINSTANCE hD3DDll = nullptr;
int d3d_dll_ref = 0;

namespace D3D
{
ID3D11Device* device = nullptr;
ID3D11Device1* device1 = nullptr;
ID3D11DeviceContext* context = nullptr;
IDXGISwapChain* swapchain = nullptr;
static ID3D11Debug* debug = nullptr;
D3D_FEATURE_LEVEL featlevel;
D3DTexture2D* backbuf = nullptr;
HWND hWnd;

std::vector<DXGI_SAMPLE_DESC> aa_modes;  // supported AA modes of the current adapter

bool bgra_textures_supported;
bool allow_tearing_supported;

#define NUM_SUPPORTED_FEATURE_LEVELS 3
const D3D_FEATURE_LEVEL supported_feature_levels[NUM_SUPPORTED_FEATURE_LEVELS] = {
    D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

unsigned int xres, yres;

bool bFrameInProgress = false;

HRESULT LoadDXGI()
{
  if (dxgi_dll_ref++ > 0)
    return S_OK;

  if (hDXGIDll)
    return S_OK;
  hDXGIDll = LoadLibraryA("dxgi.dll");
  if (!hDXGIDll)
  {
    MessageBoxA(nullptr, "Failed to load dxgi.dll", "Critical error", MB_OK | MB_ICONERROR);
    --dxgi_dll_ref;
    return E_FAIL;
  }
  PCreateDXGIFactory = (CREATEDXGIFACTORY)GetProcAddress(hDXGIDll, "CreateDXGIFactory");
  // Even though we use IDXGIFactory2 we use CreateDXGIFactory1 to create it to maintain
  // compatibility with Windows 7
  PCreateDXGIFactory1 = (CREATEDXGIFACTORY)GetProcAddress(hDXGIDll, "CreateDXGIFactory1");
  if (PCreateDXGIFactory == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for CreateDXGIFactory!", "Critical error",
                MB_OK | MB_ICONERROR);

  return S_OK;
}

HRESULT LoadD3D()
{
  if (d3d_dll_ref++ > 0)
    return S_OK;

  if (hD3DDll)
    return S_OK;
  hD3DDll = LoadLibraryA("d3d11.dll");
  if (!hD3DDll)
  {
    MessageBoxA(nullptr, "Failed to load d3d11.dll", "Critical error", MB_OK | MB_ICONERROR);
    --d3d_dll_ref;
    return E_FAIL;
  }
  PD3D11CreateDevice = (D3D11CREATEDEVICE)GetProcAddress(hD3DDll, "D3D11CreateDevice");
  if (PD3D11CreateDevice == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3D11CreateDevice!", "Critical error",
                MB_OK | MB_ICONERROR);

  PD3D11CreateDeviceAndSwapChain =
      (D3D11CREATEDEVICEANDSWAPCHAIN)GetProcAddress(hD3DDll, "D3D11CreateDeviceAndSwapChain");
  if (PD3D11CreateDeviceAndSwapChain == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3D11CreateDeviceAndSwapChain!",
                "Critical error", MB_OK | MB_ICONERROR);

  return S_OK;
}

HRESULT LoadD3DCompiler()
{
  if (d3dcompiler_dll_ref++ > 0)
    return S_OK;
  if (hD3DCompilerDll)
    return S_OK;

  // The older version of the D3D compiler cannot compile our ubershaders without various
  // graphical issues. D3DCOMPILER_DLL_A should point to d3dcompiler_47.dll, so if this fails
  // to load, inform the user that they need to update their system.
  hD3DCompilerDll = LoadLibraryA(D3DCOMPILER_DLL_A);
  if (!hD3DCompilerDll)
  {
    PanicAlertT("Failed to load %s. If you are using Windows 7, try installing the "
                "KB4019990 update package.",
                D3DCOMPILER_DLL_A);
    return E_FAIL;
  }

  PD3DReflect = (D3DREFLECT)GetProcAddress(hD3DCompilerDll, "D3DReflect");
  if (PD3DReflect == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3DReflect!", "Critical error",
                MB_OK | MB_ICONERROR);
  PD3DCompile = (pD3DCompile)GetProcAddress(hD3DCompilerDll, "D3DCompile");
  if (PD3DCompile == nullptr)
    MessageBoxA(nullptr, "GetProcAddress failed for D3DCompile!", "Critical error",
                MB_OK | MB_ICONERROR);

  return S_OK;
}

void UnloadDXGI()
{
  if (!dxgi_dll_ref)
    return;
  if (--dxgi_dll_ref != 0)
    return;

  if (hDXGIDll)
    FreeLibrary(hDXGIDll);
  hDXGIDll = nullptr;
  PCreateDXGIFactory = nullptr;
  PCreateDXGIFactory1 = nullptr;
}

void UnloadD3D()
{
  if (!d3d_dll_ref)
    return;
  if (--d3d_dll_ref != 0)
    return;

  if (hD3DDll)
    FreeLibrary(hD3DDll);
  hD3DDll = nullptr;
  PD3D11CreateDevice = nullptr;
  PD3D11CreateDeviceAndSwapChain = nullptr;
}

void UnloadD3DCompiler()
{
  if (!d3dcompiler_dll_ref)
    return;
  if (--d3dcompiler_dll_ref != 0)
    return;

  if (hD3DCompilerDll)
    FreeLibrary(hD3DCompilerDll);
  hD3DCompilerDll = nullptr;
  PD3DReflect = nullptr;
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
  HRESULT hr = PD3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
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
  PD3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, supported_feature_levels,
                     NUM_SUPPORTED_FEATURE_LEVELS, D3D11_SDK_VERSION, nullptr, &feat_level,
                     nullptr);
  return feat_level;
}

//#pragma optimize("", off)
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

HRESULT Create(HWND wnd)
{
  hWnd = wnd;
  HRESULT hr;

  RECT client;
  GetClientRect(hWnd, &client);
  xres = client.right - client.left;
  yres = client.bottom - client.top;

  hr = LoadDXGI();
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

  IDXGIFactory* factory = nullptr;
  IDXGIAdapter* adapter = nullptr;
  IDXGIOutput* output = nullptr;
  DXGI_OUTPUT_DESC out_desc;
  if (g_vr_needs_DXGIFactory1)
  {
    if (PCreateDXGIFactory1)
      hr = PCreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    else
      hr = PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
  }
  else
  {
    hr = PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
  }
  if (FAILED(hr))
    MessageBox(wnd, _T("Failed to create IDXGIFactory object"), _T("Dolphin Direct3D 11 backend"),
               MB_OK | MB_ICONERROR);

  if (g_hmd_luid)
  {
    SAFE_RELEASE(adapter);
    for (UINT iAdapter = 0; factory->EnumAdapters(iAdapter, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++iAdapter)
    {
      DXGI_ADAPTER_DESC Desc;
      adapter->GetDesc(&Desc);
      if (memcmp(&Desc.AdapterLuid, g_hmd_luid, sizeof(LUID)) == 0)
      {
        // TODO: Make this configurable
        hr = adapter->EnumOutputs(0, &output);
        if (FAILED(hr))
        {
          // try using the first one
          IDXGIAdapter* firstadapter;
          hr = factory->EnumAdapters(0, &firstadapter);
          if (!FAILED(hr))
            hr = firstadapter->EnumOutputs(0, &output);
          if (FAILED(hr))
            MessageBox(wnd,
                       _T("Failed to enumerate outputs!\n")
                       _T("This usually happens when you've set your video adapter to the Nvidia ")
                       _T("GPU in an Optimus-equipped system.\n")
                       _T("Set Dolphin to use the high-performance graphics in Nvidia's drivers ")
                       _T("instead and leave Dolphin's video adapter set to the Intel GPU."),
                       _T("Dolphin Direct3D 11 backend"), MB_OK | MB_ICONERROR);
          SAFE_RELEASE(firstadapter);
        }
        break;
      }
      SAFE_RELEASE(adapter);
    }
  }

  // Find the adapter & output (monitor) to use for fullscreen, based on the reported name of the
  // HMD's monitor.
  if (g_hmd_device_name && strlen(g_hmd_device_name) > 1)
  {
    for (UINT AdapterIndex = 0;; AdapterIndex++)
    {
      SAFE_RELEASE(adapter);

      hr = factory->EnumAdapters(AdapterIndex, &adapter);
      if (hr == DXGI_ERROR_NOT_FOUND)
        break;

      DXGI_ADAPTER_DESC Desc;
      adapter->GetDesc(&Desc);

      bool deviceNameFound = false;

      for (UINT OutputIndex = 0;; OutputIndex++)
      {
        IDXGIOutput* Output;
        hr = adapter->EnumOutputs(OutputIndex, &Output);
        if (hr == DXGI_ERROR_NOT_FOUND)
        {
          break;
        }

        memset(&out_desc, 0, sizeof(out_desc));
        Output->GetDesc(&out_desc);

        MONITORINFOEXA monitor;
        monitor.cbSize = sizeof(monitor);
        if (::GetMonitorInfoA(out_desc.Monitor, &monitor) &&
            !strcmp(monitor.szDevice, g_hmd_device_name))
        {
          deviceNameFound = true;
          output = Output;
          // FSDesktopX = monitor.rcMonitor.left;
          // FSDesktopY = monitor.rcMonitor.top;
          break;
        }
        else
        {
          SAFE_RELEASE(Output);
        }
      }

      if (output)
        break;
    }

    if (!output)
      SAFE_RELEASE(adapter);
  }

  if (!adapter)
  {
    hr = factory->EnumAdapters(g_ActiveConfig.iAdapter, &adapter);
    if (FAILED(hr))
    {
      // try using the first one
      hr = factory->EnumAdapters(0, &adapter);
      if (FAILED(hr))
        MessageBox(wnd, _T("Failed to enumerate adapters"), _T("Dolphin Direct3D 11 backend"),
                   MB_OK | MB_ICONERROR);
    }

    // TODO: Make this configurable
    hr = adapter->EnumOutputs(0, &output);
    if (FAILED(hr))
    {
      // try using the first one
      IDXGIAdapter* firstadapter;
      hr = factory->EnumAdapters(0, &firstadapter);
      if (!FAILED(hr))
        hr = firstadapter->EnumOutputs(0, &output);
      if (FAILED(hr))
        MessageBox(wnd,
                   _T("Failed to enumerate outputs!\n")
                   _T("This usually happens when you've set your video adapter to the Nvidia ")
                   _T("GPU in an Optimus-equipped system.\n")
                   _T("Set Dolphin to use the high-performance graphics in Nvidia's drivers ")
                   _T("instead and leave Dolphin's video adapter set to the Intel GPU."),
                   _T("Dolphin Direct3D 11 backend"), MB_OK | MB_ICONERROR);
      SAFE_RELEASE(firstadapter);
    }
  }

  // get supported AA modes
  aa_modes = EnumAAModes(adapter);

  if (std::find_if(aa_modes.begin(), aa_modes.end(), [](const DXGI_SAMPLE_DESC& desc) {
        return desc.Count == g_Config.iMultisamples;
      }) == aa_modes.end())
  {
    Config::SetCurrent(Config::GFX_MSAA, UINT32_C(1));
    UpdateActiveConfig();
  }

  // Check support for allow tearing, we query the interface for backwards compatibility
  UINT allow_tearing = FALSE;
  // IDXGIFactory5* factory5;
  // hr = factory->QueryInterface(&factory5);
  // if (SUCCEEDED(hr))
  // {
  //   hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing,
  //                                      sizeof(allow_tearing));
  //   factory5->Release();
  // }
  allow_tearing_supported = SUCCEEDED(hr) && allow_tearing;

  DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
  swap_chain_desc.BufferCount = 1;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.OutputWindow = wnd;
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.SampleDesc.Quality = 0;
  swap_chain_desc.Windowed =
      !SConfig::GetInstance().bFullscreen || g_ActiveConfig.bBorderlessFullscreen;
  // swap_chain_desc.Stereo = g_ActiveConfig.iStereoMode == STEREO_QUADBUFFER;

  // This flag is necessary if we want to use a flip-model swapchain without locking the framerate
  swap_chain_desc.Flags = allow_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  out_desc = {};
  output->GetDesc(&out_desc);

  DXGI_MODE_DESC mode_desc = {};
  mode_desc.Width = out_desc.DesktopCoordinates.right - out_desc.DesktopCoordinates.left;
  mode_desc.Height = out_desc.DesktopCoordinates.bottom - out_desc.DesktopCoordinates.top;
  if (g_has_hmd)
  {
    mode_desc.Width = g_hmd_window_width;
    mode_desc.Height = g_hmd_window_height;
  }
  mode_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  mode_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
  hr = output->FindClosestMatchingMode(&mode_desc, &swap_chain_desc.BufferDesc, nullptr);
  if (FAILED(hr))
    MessageBox(wnd, _T("Failed to find a supported video mode"), _T("Dolphin Direct3D 11 backend"),
               MB_OK | MB_ICONERROR);

  if (swap_chain_desc.Windowed)
  {
    // forcing buffer resolution to xres and yres..
    // this is not a problem as long as we're in windowed mode
    if (g_is_direct_mode)
    {
      swap_chain_desc.BufferDesc.Width = g_hmd_window_width;
      swap_chain_desc.BufferDesc.Height = g_hmd_window_height;
    }
    else
    {
      swap_chain_desc.BufferDesc.Width = xres;
      swap_chain_desc.BufferDesc.Height = yres;
    }
  }

  // Creating debug devices can sometimes fail if the user doesn't have the correct
  // version of the DirectX SDK. If it does, simply fallback to a non-debug device.
  if (g_Config.bEnableValidationLayer)
  {
    hr = PD3D11CreateDeviceAndSwapChain(
        adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_DEBUG, supported_feature_levels,
        NUM_SUPPORTED_FEATURE_LEVELS, D3D11_SDK_VERSION, &swap_chain_desc, &swapchain, &device,
        &featlevel, &context);
    // Debugbreak on D3D error
    if (SUCCEEDED(hr) && SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debug)))
    {
      ID3D11InfoQueue* infoQueue = nullptr;
      if (SUCCEEDED(debug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&infoQueue)))
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
    hr = PD3D11CreateDeviceAndSwapChain(
        adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
        supported_feature_levels, NUM_SUPPORTED_FEATURE_LEVELS, D3D11_SDK_VERSION, &swap_chain_desc,
        &swapchain, &device, &featlevel, &context);
  }

  if (FAILED(hr))
  {
    MessageBox(
        wnd,
        _T("Failed to initialize Direct3D.\nMake sure your video card supports at least D3D 10.0"),
        _T("Dolphin Direct3D 11 backend"), MB_OK | MB_ICONERROR);
    SAFE_RELEASE(device);
    SAFE_RELEASE(context);
    SAFE_RELEASE(swapchain);
    return E_FAIL;
  }

  hr = device->QueryInterface<ID3D11Device1>(&device1);
  if (FAILED(hr))
    WARN_LOG(VIDEO, "Missing Direct3D 11.1 support. Logical operations will not be supported.");

  // prevent DXGI from responding to Alt+Enter, unfortunately DXGI_MWA_NO_ALT_ENTER
  // does not work so we disable all monitoring of window messages. However this
  // may make it more difficult for DXGI to handle display mode changes.
  hr = factory->MakeWindowAssociation(wnd, DXGI_MWA_NO_WINDOW_CHANGES);
  if (FAILED(hr))
    MessageBox(wnd, _T("Failed to associate the window"), _T("Dolphin Direct3D 11 backend"),
               MB_OK | MB_ICONERROR);

  SetDebugObjectName(context, "device context");
  SAFE_RELEASE(factory);
  SAFE_RELEASE(output);
  SAFE_RELEASE(adapter);

  if (SConfig::GetInstance().bFullscreen && !g_ActiveConfig.bBorderlessFullscreen)
  {
    swapchain->SetFullscreenState(true, nullptr);
    swapchain->ResizeBuffers(0, xres, yres, DXGI_FORMAT_R8G8B8A8_UNORM, swap_chain_desc.Flags);
  }

  ID3D11Texture2D* buf;
  hr = swapchain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&buf);
  if (FAILED(hr))
  {
    MessageBox(wnd, _T("Failed to get swapchain buffer"), _T("Dolphin Direct3D 11 backend"),
               MB_OK | MB_ICONERROR);
    SAFE_RELEASE(device);
    SAFE_RELEASE(context);
    SAFE_RELEASE(swapchain);
    return E_FAIL;
  }
  backbuf = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
  SAFE_RELEASE(buf);
  CHECK(backbuf != nullptr, "Create back buffer texture");
  SetDebugObjectName(backbuf->GetTex(), "backbuffer texture");
  SetDebugObjectName(backbuf->GetRTV(), "backbuffer render target view");

  context->OMSetRenderTargets(1, &backbuf->GetRTV(), nullptr);

  // BGRA textures are easier to deal with in TextureCache, but might not be supported by the
  // hardware
  UINT format_support;
  device->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM, &format_support);
  bgra_textures_supported = (format_support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
  g_Config.backend_info.bSupportsST3CTextures = SupportsS3TCTextures(device);
  g_Config.backend_info.bSupportsBPTCTextures = SupportsBPTCTextures(device);

  stateman = new StateManager;
  return S_OK;
}
//#pragma optimize("", on)

void Close()
{
  // we can't release the swapchain while in fullscreen.
  swapchain->SetFullscreenState(false, nullptr);

  // release all bound resources
  context->ClearState();
  SAFE_RELEASE(backbuf);
  SAFE_RELEASE(swapchain);
  SAFE_DELETE(stateman);
  context->Flush();  // immediately destroy device objects

  SAFE_RELEASE(context);
  SAFE_RELEASE(device1);
  ULONG references = device->Release();

#if defined(_DEBUG) || defined(DEBUGFAST)
  if (debug)
  {
    --references;  // the debug interface increases the refcount of the device, subtract that.
    if (references)
    {
      // print out alive objects, but only if we actually have pending references
      // note this will also print out internal live objects to the debug console
      debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
    }
    SAFE_RELEASE(debug)
  }
#endif

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
  if (featlevel == D3D_FEATURE_LEVEL_11_0)
    return "vs_5_0";
  else if (featlevel == D3D_FEATURE_LEVEL_10_1)
    return "vs_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "vs_4_0";
}

const char* GeometryShaderVersionString()
{
  if (featlevel == D3D_FEATURE_LEVEL_11_0)
    return "gs_5_0";
  else if (featlevel == D3D_FEATURE_LEVEL_10_1)
    return "gs_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "gs_4_0";
}

const char* PixelShaderVersionString()
{
  if (featlevel == D3D_FEATURE_LEVEL_11_0)
    return "ps_5_0";
  else if (featlevel == D3D_FEATURE_LEVEL_10_1)
    return "ps_4_1";
  else /*if(featlevel == D3D_FEATURE_LEVEL_10_0)*/
    return "ps_4_0";
}

D3DTexture2D*& GetBackBuffer()
{
  return backbuf;
}
unsigned int GetBackBufferWidth()
{
  return xres;
}
unsigned int GetBackBufferHeight()
{
  return yres;
}

bool BGRATexturesSupported()
{
  return bgra_textures_supported;
}

bool AllowTearingSupported()
{
  return allow_tearing_supported;
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

void Reset()
{
  // release all back buffer references
  SAFE_RELEASE(backbuf);

  UINT swap_chain_flags = AllowTearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  // resize swapchain buffers
  RECT client;
  GetClientRect(hWnd, &client);
  xres = client.right - client.left;
  yres = client.bottom - client.top;
  D3D::swapchain->ResizeBuffers(0, xres, yres, DXGI_FORMAT_R8G8B8A8_UNORM, swap_chain_flags);

  // recreate back buffer texture
  ID3D11Texture2D* buf;
  HRESULT hr = swapchain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&buf);
  if (FAILED(hr))
  {
    MessageBox(hWnd, _T("Failed to get swapchain buffer"), _T("Dolphin Direct3D 11 backend"),
               MB_OK | MB_ICONERROR);
    SAFE_RELEASE(device);
    SAFE_RELEASE(context);
    SAFE_RELEASE(swapchain);
    return;
  }
  backbuf = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
  SAFE_RELEASE(buf);
  CHECK(backbuf != nullptr, "Create back buffer texture");
  SetDebugObjectName(backbuf->GetTex(), "backbuffer texture");
  SetDebugObjectName(backbuf->GetRTV(), "backbuffer render target view");
}

bool BeginFrame()
{
  if (bFrameInProgress)
  {
    PanicAlert("BeginFrame called although a frame is already in progress");
    return false;
  }
  bFrameInProgress = true;
  return (device != nullptr);
}

void EndFrame()
{
  if (!bFrameInProgress)
  {
    PanicAlert("EndFrame called although no frame is in progress");
    return;
  }
  bFrameInProgress = false;
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

  // if (swapchain->IsTemporaryMonoSupported() && g_ActiveConfig.iStereoMode != STEREO_QUADBUFFER)
  //  present_flags |= DXGI_PRESENT_STEREO_TEMPORARY_MONO;

  // TODO: Is 1 the correct value for vsyncing?
  swapchain->Present((UINT)g_ActiveConfig.IsVSync(), present_flags);
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
