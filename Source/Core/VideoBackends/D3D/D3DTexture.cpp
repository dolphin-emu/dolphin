// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DTexture.h"

namespace DX11
{
bool D3DTexture2D::Initialize(DXGI_FORMAT format, unsigned int width, unsigned int height,
                              D3D11_BIND_FLAG bind, D3D11_USAGE usage, unsigned int levels,
                              unsigned int slices, const D3D11_SUBRESOURCE_DATA* data)
{
  ComPtr<ID3D11Texture2D> pTexture;

  D3D11_CPU_ACCESS_FLAG cpuflags;
  if (usage == D3D11_USAGE_STAGING)
    cpuflags = (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ);
  else if (usage == D3D11_USAGE_DYNAMIC)
    cpuflags = D3D11_CPU_ACCESS_WRITE;
  else
    cpuflags = (D3D11_CPU_ACCESS_FLAG)0;
  D3D11_TEXTURE2D_DESC texdesc =
      CD3D11_TEXTURE2D_DESC(format, width, height, slices, levels, bind, usage, cpuflags);
  HRESULT hr = D3D::device->CreateTexture2D(&texdesc, data, &pTexture);
  if (FAILED(hr))
  {
    PanicAlert("Failed to create texture at %s, line %d: hr=%#x\n", __FILE__, __LINE__, hr);
    return false;
  }

  return InitializeFromExisting(pTexture.Get(), bind);
}

bool D3DTexture2D::InitializeFromExisting(ID3D11Texture2D* tex, D3D11_BIND_FLAG bind,
                                          DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format,
                                          DXGI_FORMAT rtv_format, bool multisampled)
{
  Reset();
  m_tex = tex;
  bool ok = (m_tex != nullptr);

  HRESULT hr;
  if (bind & D3D11_BIND_SHADER_RESOURCE)
  {
    D3D11_SRV_DIMENSION srv_dim =
        multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc =
        CD3D11_SHADER_RESOURCE_VIEW_DESC(srv_dim, srv_format);
    hr = D3D::device->CreateShaderResourceView(tex, &srv_desc, &m_srv);
    ok &= SUCCEEDED(hr);
  }
  if (bind & D3D11_BIND_RENDER_TARGET)
  {
    D3D11_RTV_DIMENSION rtv_dim =
        multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(rtv_dim, rtv_format);
    hr = D3D::device->CreateRenderTargetView(tex, &rtv_desc, &m_rtv);
    ok &= SUCCEEDED(hr);
  }
  if (bind & D3D11_BIND_DEPTH_STENCIL)
  {
    D3D11_DSV_DIMENSION dsv_dim =
        multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(dsv_dim, dsv_format);
    hr = D3D::device->CreateDepthStencilView(tex, &dsv_desc, &m_dsv);
    ok &= SUCCEEDED(hr);
  }

  return ok;
}

ID3D11Texture2D* D3DTexture2D::GetTex() const
{
  return m_tex.Get();
}
ID3D11ShaderResourceView* D3DTexture2D::GetSRV() const
{
  return m_srv.Get();
}
ID3D11RenderTargetView* D3DTexture2D::GetRTV() const
{
  return m_rtv.Get();
}
ID3D11DepthStencilView* D3DTexture2D::GetDSV() const
{
  return m_dsv.Get();
}

void D3DTexture2D::Reset()
{
  m_dsv.Reset();
  m_rtv.Reset();
  m_srv.Reset();
  m_tex.Reset();
}

bool D3DTexture2D::IsValid() const
{
  return m_tex != nullptr;
}

}  // namespace DX11
