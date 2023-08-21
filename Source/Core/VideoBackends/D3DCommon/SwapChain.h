// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <dxgi1_5.h>
#include <wrl/client.h>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"
#include "VideoCommon/TextureConfig.h"

namespace D3DCommon
{
class SwapChain
{
public:
  SwapChain(const WindowSystemInfo& wsi, IDXGIFactory* dxgi_factory, IUnknown* d3d_device);
  virtual ~SwapChain();

  // Sufficient buffers for triple buffering.
  static const u32 SWAP_CHAIN_BUFFER_COUNT = 3;

  // Returns true if the stereo mode is quad-buffering.
  static bool WantsStereo();

  IDXGISwapChain* GetDXGISwapChain() const { return m_swap_chain.Get(); }
  AbstractTextureFormat GetFormat() const { return m_texture_format; }
  u32 GetWidth() const { return m_width; }
  u32 GetHeight() const { return m_height; }

  // Mode switches.
  bool GetFullscreen() const;
  void SetFullscreen(bool request);

  // Checks for loss of exclusive fullscreen.
  bool CheckForFullscreenChange();

  // Presents the swap chain to the screen.
  virtual bool Present();

  bool ChangeSurface(void* native_handle);
  bool ResizeSwapChain();
  void SetStereo(bool stereo);

protected:
  u32 GetSwapChainFlags() const;
  bool CreateSwapChain(bool stereo);
  void DestroySwapChain();

  virtual bool CreateSwapChainBuffers() = 0;
  virtual void DestroySwapChainBuffers() = 0;

  WindowSystemInfo m_wsi;
  Microsoft::WRL::ComPtr<IDXGIFactory> m_dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGISwapChain> m_swap_chain;
  Microsoft::WRL::ComPtr<IUnknown> m_d3d_device;
  AbstractTextureFormat m_texture_format = AbstractTextureFormat::RGBA8;

  u32 m_width = 1;
  u32 m_height = 1;

  bool m_stereo = false;
  bool m_allow_tearing_supported = false;
  bool m_has_fullscreen = false;
  bool m_fullscreen_request = false;
};

}  // namespace D3DCommon
