// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/D3D12SwapChain.h"
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoBackends/D3D12/DX12Texture.h"

namespace DX12
{
SwapChain::SwapChain(const WindowSystemInfo& wsi, IDXGIFactory* dxgi_factory,
                     ID3D12CommandQueue* d3d_command_queue)
    : D3DCommon::SwapChain(wsi, dxgi_factory, d3d_command_queue)
{
}

SwapChain::~SwapChain() = default;

std::unique_ptr<SwapChain> SwapChain::Create(const WindowSystemInfo& wsi)
{
  std::unique_ptr<SwapChain> swap_chain = std::make_unique<SwapChain>(
      wsi, g_dx_context->GetDXGIFactory(), g_dx_context->GetCommandQueue());
  if (!swap_chain->CreateSwapChain(WantsStereo()))
    return nullptr;

  return swap_chain;
}

bool SwapChain::CreateSwapChainBuffers()
{
  for (u32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
  {
    ComPtr<ID3D12Resource> resource;
    HRESULT hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&resource));
    CHECK(SUCCEEDED(hr), "Get swap chain buffer");

    BufferResources buffer;
    buffer.texture = DXTexture::CreateAdopted(resource.Get());
    CHECK(buffer.texture, "Create swap chain buffer texture");
    if (!buffer.texture)
      return false;

    buffer.framebuffer = DXFramebuffer::Create(buffer.texture.get(), nullptr);
    CHECK(buffer.texture, "Create swap chain buffer framebuffer");
    if (!buffer.framebuffer)
      return false;

    m_buffers.push_back(std::move(buffer));
  }

  m_current_buffer = 0;
  return true;
}

void SwapChain::DestroySwapChainBuffers()
{
  // Swap chain textures must be released before it can be resized, therefore we need to destroy all
  // of them immediately, and not place them onto the deferred desturction queue.
  for (BufferResources& res : m_buffers)
  {
    res.framebuffer.reset();
    res.texture->DestroyResource();
    res.texture.release();
  }
  m_buffers.clear();
}

bool SwapChain::Present()
{
  if (!D3DCommon::SwapChain::Present())
    return false;

  m_current_buffer = (m_current_buffer + 1) % SWAP_CHAIN_BUFFER_COUNT;
  return true;
}
}  // namespace DX12
