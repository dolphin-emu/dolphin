// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3DCommon/SwapChain.h"
#include "VideoCommon/TextureConfig.h"

namespace DX11
{
class DXTexture;
class DXFramebuffer;

class SwapChain : public D3DCommon::SwapChain
{
public:
  SwapChain(const WindowSystemInfo& wsi, IDXGIFactory* dxgi_factory, ID3D11Device* d3d_device);
  ~SwapChain();

  static std::unique_ptr<SwapChain> Create(const WindowSystemInfo& wsi);

  DXTexture* GetTexture() const { return m_texture.get(); }
  DXFramebuffer* GetFramebuffer() const { return m_framebuffer.get(); }

protected:
  bool CreateSwapChainBuffers() override;
  void DestroySwapChainBuffers() override;

private:
  // The runtime takes care of renaming the buffers.
  std::unique_ptr<DXTexture> m_texture;
  std::unique_ptr<DXFramebuffer> m_framebuffer;
};

}  // namespace DX11
