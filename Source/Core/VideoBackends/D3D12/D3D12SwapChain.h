// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3DCommon/SwapChain.h"
#include "VideoCommon/TextureConfig.h"

namespace DX12
{
class DXTexture;
class DXFramebuffer;

class SwapChain : public D3DCommon::SwapChain
{
public:
  SwapChain(const WindowSystemInfo& wsi, IDXGIFactory* dxgi_factory,
            ID3D12CommandQueue* d3d_command_queue);
  ~SwapChain();

  static std::unique_ptr<SwapChain> Create(const WindowSystemInfo& wsi);

  bool Present() override;

  DXTexture* GetCurrentTexture() const { return m_buffers[m_current_buffer].texture.get(); }
  DXFramebuffer* GetCurrentFramebuffer() const
  {
    return m_buffers[m_current_buffer].framebuffer.get();
  }

protected:
  bool CreateSwapChainBuffers() override;
  void DestroySwapChainBuffers() override;

private:
  struct BufferResources
  {
    std::unique_ptr<DXTexture> texture;
    std::unique_ptr<DXFramebuffer> framebuffer;
  };

  std::vector<BufferResources> m_buffers;
  u32 m_current_buffer = 0;
};

}  // namespace DX12
