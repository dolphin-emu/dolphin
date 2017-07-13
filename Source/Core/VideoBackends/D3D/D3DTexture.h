// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"

namespace DX11
{
class D3DTexture2D
{
public:
  bool Initialize(DXGI_FORMAT format, unsigned int width, unsigned int height, D3D11_BIND_FLAG bind,
                  D3D11_USAGE usage = D3D11_USAGE_DEFAULT, unsigned int levels = 1,
                  unsigned int slices = 1, const D3D11_SUBRESOURCE_DATA* data = nullptr);

  // Initialize from an existing ID3D11Texture2D. This takes ownership of the texture.
  bool InitializeFromExisting(ComPtr<ID3D11Texture2D>&& tex, D3D11_BIND_FLAG bind,
                              DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN,
                              DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN,
                              DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN,
                              bool multisampled = false);

  void Reset();
  bool IsValid() const;

  // Note that Get* functions are marked const, despite their possibly being passed to functions
  // that modify the texture.
  // Essentially, D3DTexture2D owns and manages the texture's pointers, not the actual contents of
  // the texture.
  ID3D11Texture2D* GetTex() const;
  ID3D11ShaderResourceView* GetSRV() const;
  ID3D11RenderTargetView* GetRTV() const;
  ID3D11DepthStencilView* GetDSV() const;

private:
  ComPtr<ID3D11Texture2D> m_tex;
  ComPtr<ID3D11ShaderResourceView> m_srv;
  ComPtr<ID3D11RenderTargetView> m_rtv;
  ComPtr<ID3D11DepthStencilView> m_dsv;
};

}  // namespace DX11
