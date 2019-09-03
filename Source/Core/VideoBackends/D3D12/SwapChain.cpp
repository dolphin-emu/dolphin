// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/SwapChain.h"
#include "VideoBackends/D3D12/DXContext.h"
#include "VideoBackends/D3D12/DXTexture.h"

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
  if (!g_dx_context->Is12On7Device())
  {
    if (!swap_chain->CreateSwapChain(WantsStereo()))
      return nullptr;
  }
  else
  {
    // 12on7 doesn't create a DXGI swap chain.
    swap_chain->UpdateSizeFromWindow();
    if (!swap_chain->CreateSwapChainBuffers())
      return nullptr;
  }

  return swap_chain;
}

bool SwapChain::CreateSwapChainBuffers()
{
  for (u32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
  {
    ComPtr<ID3D12Resource> resource;
    if (!g_dx_context->Is12On7Device())
    {
      HRESULT hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&resource));
      CHECK(SUCCEEDED(hr), "Get swap chain buffer");
    }
    else
    {
      // has to go through here because we can't use typeless formats
      constexpr D3D12_HEAP_PROPERTIES heap_properties = {D3D12_HEAP_TYPE_DEFAULT};
      const D3D12_RESOURCE_DESC desc = {
          D3D12_RESOURCE_DIMENSION_TEXTURE2D,
          0,
          m_width,
          m_height,
          static_cast<u16>(m_stereo ? 2 : 1),
          static_cast<u16>(1),
          D3DCommon::GetDXGIFormatForAbstractFormat(m_texture_format, false),
          {1, 0},
          D3D12_TEXTURE_LAYOUT_UNKNOWN,
          D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};
      D3D12_CLEAR_VALUE optimized_clear_value = {};
      optimized_clear_value.Format = desc.Format;

      HRESULT hr = g_dx_context->GetDevice()->CreateCommittedResource(
          &heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
          &optimized_clear_value, IID_PPV_ARGS(resource.GetAddressOf()));
      CHECK(SUCCEEDED(hr), "Create swap chain backbuffer");
    }

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

void SwapChain::UpdateSizeFromWindow()
{
  // No swap chain to query size from, so work from the client area.
  RECT client_rc = {};
  GetClientRect(static_cast<HWND>(m_wsi.render_surface), &client_rc);
  m_width = std::max<u32>(client_rc.right - client_rc.left, 1u);
  m_height = std::max<u32>(client_rc.bottom - client_rc.top, 1u);
}

bool SwapChain::Present()
{
  if (!D3DCommon::SwapChain::Present())
    return false;

  MoveToNextBuffer();
  return true;
}

bool SwapChain::CheckForFullscreenChange()
{
  if (!g_dx_context->Is12On7Device())
    return D3DCommon::SwapChain::CheckForFullscreenChange();

  // 12on7 does not support exclusive fullscreen.
  return false;
}

bool SwapChain::ChangeSurface(void* native_handle)
{
  if (!g_dx_context->Is12On7Device())
    return D3DCommon::SwapChain::ChangeSurface(native_handle);

  m_wsi.render_surface = native_handle;
  return ResizeSwapChain();
}

bool SwapChain::ResizeSwapChain()
{
  if (!g_dx_context->Is12On7Device())
    return D3DCommon::SwapChain::ResizeSwapChain();

  UpdateSizeFromWindow();
  DestroySwapChainBuffers();
  return CreateSwapChainBuffers();
}

void SwapChain::SetStereo(bool stereo)
{
  if (g_dx_context->Is12On7Device())
    return;

  return D3DCommon::SwapChain::SetStereo(stereo);
}

}  // namespace DX12
