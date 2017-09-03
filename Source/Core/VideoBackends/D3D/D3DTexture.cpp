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
D3DTexture2D* D3DTexture2D::Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind,
                                   D3D11_USAGE usage, DXGI_FORMAT fmt, unsigned int levels,
                                   unsigned int slices, D3D11_SUBRESOURCE_DATA* data)
{
  ID3D11Texture2D* pTexture = nullptr;
  HRESULT hr;

  D3D11_CPU_ACCESS_FLAG cpuflags;
  if (usage == D3D11_USAGE_STAGING)
    cpuflags = (D3D11_CPU_ACCESS_FLAG)((int)D3D11_CPU_ACCESS_WRITE | (int)D3D11_CPU_ACCESS_READ);
  else if (usage == D3D11_USAGE_DYNAMIC)
    cpuflags = D3D11_CPU_ACCESS_WRITE;
  else
    cpuflags = (D3D11_CPU_ACCESS_FLAG)0;
  D3D11_TEXTURE2D_DESC texdesc =
      CD3D11_TEXTURE2D_DESC(fmt, width, height, slices, levels, bind, usage, cpuflags);
  hr = D3D::device->CreateTexture2D(&texdesc, data, &pTexture);
  if (FAILED(hr))
  {
    PanicAlert("Failed to create texture at %s, line %d: hr=%#x\n", __FILE__, __LINE__, hr);
    return nullptr;
  }

  D3DTexture2D* ret = new D3DTexture2D(pTexture, bind);
  SAFE_RELEASE(pTexture);
  return ret;
}

void D3DTexture2D::AddRef()
{
  ++ref;
}

UINT D3DTexture2D::Release()
{
  --ref;
  if (ref == 0)
  {
    delete this;
    return 0;
  }
  return ref;
}

ID3D11Texture2D*& D3DTexture2D::GetTex()
{
  return tex;
}
ID3D11ShaderResourceView*& D3DTexture2D::GetSRV()
{
  return srv;
}
ID3D11RenderTargetView*& D3DTexture2D::GetRTV()
{
  return rtv;
}
ID3D11DepthStencilView*& D3DTexture2D::GetDSV()
{
  return dsv;
}

D3DTexture2D::D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind, DXGI_FORMAT srv_format,
                           DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format, bool multisampled)
    : tex{texptr}
{
  D3D11_SRV_DIMENSION srv_dim =
      multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  D3D11_DSV_DIMENSION dsv_dim =
      multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
  D3D11_RTV_DIMENSION rtv_dim =
      multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(srv_dim, srv_format);
  D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(dsv_dim, dsv_format);
  D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(rtv_dim, rtv_format);
  if (bind & D3D11_BIND_SHADER_RESOURCE)
    D3D::device->CreateShaderResourceView(tex, &srv_desc, &srv);
  if (bind & D3D11_BIND_RENDER_TARGET)
    D3D::device->CreateRenderTargetView(tex, &rtv_desc, &rtv);
  if (bind & D3D11_BIND_DEPTH_STENCIL)
    D3D::device->CreateDepthStencilView(tex, &dsv_desc, &dsv);
  tex->AddRef();
}

D3DTexture2D::~D3DTexture2D()
{
  SAFE_RELEASE(srv);
  SAFE_RELEASE(rtv);
  SAFE_RELEASE(dsv);
  SAFE_RELEASE(tex);
}

}  // namespace DX11
