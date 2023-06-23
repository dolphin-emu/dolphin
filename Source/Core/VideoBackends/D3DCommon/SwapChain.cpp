// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3DCommon/SwapChain.h"

#include <algorithm>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/HRWrap.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoCommon/VideoConfig.h"

static bool IsTearingSupported(IDXGIFactory2* dxgi_factory)
{
  Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
  if (FAILED(dxgi_factory->QueryInterface(IID_PPV_ARGS(&factory5))))
    return false;

  UINT allow_tearing = 0;
  return SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing,
                                                 sizeof(allow_tearing))) &&
         allow_tearing != 0;
}

static bool GetFullscreenState(IDXGISwapChain* swap_chain)
{
  BOOL fs = FALSE;
  return SUCCEEDED(swap_chain->GetFullscreenState(&fs, nullptr)) && fs;
}

namespace D3DCommon
{
SwapChain::SwapChain(const WindowSystemInfo& wsi, IDXGIFactory* dxgi_factory, IUnknown* d3d_device)
    : m_wsi(wsi), m_dxgi_factory(dxgi_factory), m_d3d_device(d3d_device)
{
}

SwapChain::~SwapChain()
{
  // Can't destroy swap chain while it's fullscreen.
  if (m_swap_chain && GetFullscreenState(m_swap_chain.Get()))
    m_swap_chain->SetFullscreenState(FALSE, nullptr);
}

bool SwapChain::WantsStereo()
{
  return g_ActiveConfig.stereo_mode == StereoMode::QuadBuffer;
}

bool SwapChain::WantsHDR()
{
  return g_ActiveConfig.bHDR;
}

u32 SwapChain::GetSwapChainFlags() const
{
  // This flag is necessary if we want to use a flip-model swapchain without locking the framerate
  return m_allow_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
}

bool SwapChain::CreateSwapChain(bool stereo, bool hdr)
{
  RECT client_rc;
  if (GetClientRect(static_cast<HWND>(m_wsi.render_surface), &client_rc))
  {
    m_width = client_rc.right - client_rc.left;
    m_height = client_rc.bottom - client_rc.top;
  }

  m_stereo = false;
  m_hdr = false;

  // Try using the Win8 version if available.
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory2;
  HRESULT hr = m_dxgi_factory.As(&dxgi_factory2);
  if (SUCCEEDED(hr))
  {
    m_allow_tearing_supported = IsTearingSupported(dxgi_factory2.Get());

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = m_width;
    swap_chain_desc.Height = m_height;
    swap_chain_desc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.Format = GetDXGIFormatForAbstractFormat(m_texture_format, false);

    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.Stereo = stereo;
    swap_chain_desc.Flags = GetSwapChainFlags();

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;
    hr = dxgi_factory2->CreateSwapChainForHwnd(m_d3d_device.Get(),
                                               static_cast<HWND>(m_wsi.render_surface),
                                               &swap_chain_desc, nullptr, nullptr, &swap_chain1);
    if (FAILED(hr))
    {
      // Flip-model discard swapchains aren't supported on Windows 8, so here we fall back to
      // a sequential swapchain
      swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      hr = dxgi_factory2->CreateSwapChainForHwnd(m_d3d_device.Get(),
                                                 static_cast<HWND>(m_wsi.render_surface),
                                                 &swap_chain_desc, nullptr, nullptr, &swap_chain1);
    }

    m_swap_chain = swap_chain1;
  }

  // Flip-model swapchains aren't supported on Windows 7, so here we fall back to a legacy
  // BitBlt-model swapchain. Note that this won't work for DX12, but systems which don't
  // support the newer DXGI interface aren't going to support DX12 anyway.
  if (FAILED(hr))
  {
    hdr = false;

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Width = m_width;
    desc.BufferDesc.Height = m_height;
    desc.BufferDesc.Format = GetDXGIFormatForAbstractFormat(m_texture_format, false);
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    desc.OutputWindow = static_cast<HWND>(m_wsi.render_surface);
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.Flags = 0;

    m_allow_tearing_supported = false;
    hr = m_dxgi_factory->CreateSwapChain(m_d3d_device.Get(), &desc, &m_swap_chain);
  }

  if (FAILED(hr))
  {
    PanicAlertFmt("Failed to create swap chain: {}", Common::HRWrap(hr));
    return false;
  }

  // We handle fullscreen ourselves.
  hr = m_dxgi_factory->MakeWindowAssociation(static_cast<HWND>(m_wsi.render_surface),
                                             DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
  if (FAILED(hr))
    WARN_LOG_FMT(VIDEO, "MakeWindowAssociation() failed: {}", Common::HRWrap(hr));

  m_stereo = stereo;

  if (hdr)
  {
    // Only try to activate HDR here, to avoid failing when creating the swapchain
    // (we can't know if the format is supported upfront)
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_chain4;
    hr = m_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain4));
    if (SUCCEEDED(hr))
    {
      UINT color_space_support = 0;
      // Note that this should succeed even if HDR is not currently engaged on the monitor,
      // but it should display fine nonetheless.
      // We need to check for DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 as checking for
      // scRGB always returns false (DX bug).
      hr = swap_chain4->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,
                                               &color_space_support);
      if (SUCCEEDED(hr) && (color_space_support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
      {
        hr = swap_chain4->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, 0, 0,
                                        GetDXGIFormatForAbstractFormat(m_texture_format_hdr, false),
                                        GetSwapChainFlags());
        if (SUCCEEDED(hr))
        {
          hr = swap_chain4->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
          if (SUCCEEDED(hr))
            m_hdr = hdr;
        }
      }
    }
  }

  if (!CreateSwapChainBuffers())
  {
    PanicAlertFmt("Failed to create swap chain buffers");
    DestroySwapChainBuffers();
    m_swap_chain.Reset();
    return false;
  }

  return true;
}

void SwapChain::DestroySwapChain()
{
  DestroySwapChainBuffers();

  // Can't destroy swap chain while it's fullscreen.
  if (m_swap_chain && GetFullscreenState(m_swap_chain.Get()))
    m_swap_chain->SetFullscreenState(FALSE, nullptr);

  m_swap_chain.Reset();
}

bool SwapChain::ResizeSwapChain()
{
  DestroySwapChainBuffers();

  // The swap chain fills up the size of the window if no size is specified
  HRESULT hr = m_swap_chain->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, 0, 0, DXGI_FORMAT_UNKNOWN,
                                           GetSwapChainFlags());

  if (FAILED(hr))
    WARN_LOG_FMT(VIDEO, "ResizeBuffers() failed: {}", Common::HRWrap(hr));

  Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_chain4;
  hr = m_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain4));
  if (SUCCEEDED(hr))
    hr = swap_chain4->SetColorSpace1(m_hdr ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 :
                                             DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);

  DXGI_SWAP_CHAIN_DESC desc;
  if (SUCCEEDED(m_swap_chain->GetDesc(&desc)))
  {
    m_width = desc.BufferDesc.Width;
    m_height = desc.BufferDesc.Height;
  }

  return CreateSwapChainBuffers();
}

void SwapChain::SetStereo(bool stereo)
{
  if (m_stereo == stereo)
    return;

  DestroySwapChain();
  // Do not try to re-activate HDR here if it had already failed
  if (!CreateSwapChain(stereo, m_hdr))
  {
    PanicAlertFmt("Failed to switch swap chain stereo mode");
    CreateSwapChain(false, false);
  }
}

void SwapChain::SetHDR(bool hdr)
{
  if (m_hdr == hdr)
    return;

  // NOTE: as an optimization here we could just call "ResizeSwapChain()"
  // by adding some code to check if we could change the format to HDR.

  DestroySwapChain();
  // Do not try to re-activate stereo mode here if it had already failed
  if (!CreateSwapChain(m_stereo, hdr))
  {
    PanicAlertFmt("Failed to switch swap chain SDR/HDR mode");
    CreateSwapChain(false, false);
  }
}

bool SwapChain::GetFullscreen() const
{
  return GetFullscreenState(m_swap_chain.Get());
}

void SwapChain::SetFullscreen(bool request)
{
  m_swap_chain->SetFullscreenState(request, nullptr);
}

bool SwapChain::CheckForFullscreenChange()
{
  if (m_fullscreen_request != m_has_fullscreen)
  {
    HRESULT hr = m_swap_chain->SetFullscreenState(m_fullscreen_request, nullptr);
    if (SUCCEEDED(hr))
    {
      m_has_fullscreen = m_fullscreen_request;
      return true;
    }
  }

  const bool new_fullscreen_state = GetFullscreenState(m_swap_chain.Get());
  if (new_fullscreen_state != m_has_fullscreen)
  {
    m_has_fullscreen = new_fullscreen_state;
    m_fullscreen_request = new_fullscreen_state;
    return true;
  }

  return false;
}

bool SwapChain::Present()
{
  // When using sync interval 0, it is recommended to always pass the tearing flag when it is
  // supported, even when presenting in windowed mode. However, this flag cannot be used if the app
  // is in fullscreen mode as a result of calling SetFullscreenState.
  UINT present_flags = 0;
  if (m_allow_tearing_supported && !g_ActiveConfig.bVSyncActive && !m_has_fullscreen)
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;

  HRESULT hr = m_swap_chain->Present(static_cast<UINT>(g_ActiveConfig.bVSyncActive), present_flags);
  if (FAILED(hr))
  {
    WARN_LOG_FMT(VIDEO, "Swap chain present failed: {}", Common::HRWrap(hr));
    return false;
  }

  return true;
}

bool SwapChain::ChangeSurface(void* native_handle)
{
  DestroySwapChain();
  m_wsi.render_surface = native_handle;
  // We only keep the swap chain settings (HDR/Stereo) that had successfully applied beofre
  return CreateSwapChain(m_stereo, m_hdr);
}

}  // namespace D3DCommon
