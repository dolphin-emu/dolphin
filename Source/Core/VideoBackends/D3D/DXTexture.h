// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <d3d11_4.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

#ifdef __MINGW32__
#include <cstring>
#ifndef D3D11_MIN_DEPTH
#define D3D11_MIN_DEPTH 0.0f
#endif
#ifndef D3D11_MAX_DEPTH
#define D3D11_MAX_DEPTH 1.0f
#endif

struct CD3D11_RECT : public D3D11_RECT
{
  CD3D11_RECT(LONG l, LONG t, LONG r, LONG b)
  {
    left = l;
    top = t;
    right = r;
    bottom = b;
  }
};

struct CD3D11_VIEWPORT : public D3D11_VIEWPORT
{
  CD3D11_VIEWPORT(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minD = D3D11_MIN_DEPTH,
                  FLOAT maxD = D3D11_MAX_DEPTH)
  {
    TopLeftX = x;
    TopLeftY = y;
    Width = w;
    Height = h;
    MinDepth = minD;
    MaxDepth = maxD;
  }
};

struct CD3D11_BOX : public D3D11_BOX
{
  CD3D11_BOX(UINT l, UINT t, UINT f, UINT r, UINT b, UINT ba)
  {
    left = l;
    top = t;
    front = f;
    right = r;
    bottom = b;
    back = ba;
  }
};

// UAV desc wrapper
struct CD3D11_UNORDERED_ACCESS_VIEW_DESC : public D3D11_UNORDERED_ACCESS_VIEW_DESC
{
  CD3D11_UNORDERED_ACCESS_VIEW_DESC() {}

  explicit CD3D11_UNORDERED_ACCESS_VIEW_DESC(const D3D11_UNORDERED_ACCESS_VIEW_DESC& o)
      : D3D11_UNORDERED_ACCESS_VIEW_DESC(o)
  {
  }

  // Generic initializer by dimension
  CD3D11_UNORDERED_ACCESS_VIEW_DESC(DXGI_FORMAT fmt, D3D11_UAV_DIMENSION dim, UINT mipSlice = 0,
                                    UINT firstArraySlice = 0, UINT arraySize = (UINT)-1)
  {
    std::memset(this, 0, sizeof(*this));
    Format = fmt;
    ViewDimension = dim;
    switch (dim)
    {
    case D3D11_UAV_DIMENSION_BUFFER:
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      Texture1D.MipSlice = mipSlice;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      Texture1DArray.MipSlice = mipSlice;
      Texture1DArray.FirstArraySlice = firstArraySlice;
      Texture1DArray.ArraySize = arraySize;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      Texture2D.MipSlice = mipSlice;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      Texture2DArray.MipSlice = mipSlice;
      Texture2DArray.FirstArraySlice = firstArraySlice;
      Texture2DArray.ArraySize = arraySize;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      Texture3D.MipSlice = mipSlice;
      Texture3D.WSize = arraySize;
      break;
    default:
      break;
    }
  }

  CD3D11_UNORDERED_ACCESS_VIEW_DESC(ID3D11Texture2D* pTex2D, D3D11_UAV_DIMENSION dim,
                                    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN, UINT mipSlice = 0,
                                    UINT firstArraySlice = 0, UINT arraySize = (UINT)-1)
  {
    std::memset(this, 0, sizeof(*this));
    ViewDimension = dim;

    D3D11_TEXTURE2D_DESC tex{};
    if (pTex2D)
      pTex2D->GetDesc(&tex);

    if (fmt == DXGI_FORMAT_UNKNOWN)
      fmt = tex.Format;
    if (arraySize == (UINT)-1 && (dim == D3D11_UAV_DIMENSION_TEXTURE2DARRAY))
      arraySize = tex.ArraySize ? (tex.ArraySize - firstArraySlice) : 0;

    Format = fmt;

    switch (dim)
    {
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      Texture2D.MipSlice = mipSlice;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      Texture2DArray.MipSlice = mipSlice;
      Texture2DArray.FirstArraySlice = firstArraySlice;
      Texture2DArray.ArraySize = arraySize;
      break;
    default:
      break;
    }
  }
};

struct CD3D11_DEPTH_STENCIL_VIEW_DESC : public D3D11_DEPTH_STENCIL_VIEW_DESC
{
  CD3D11_DEPTH_STENCIL_VIEW_DESC() {}

  explicit CD3D11_DEPTH_STENCIL_VIEW_DESC(const D3D11_DEPTH_STENCIL_VIEW_DESC& o)
      : D3D11_DEPTH_STENCIL_VIEW_DESC(o)
  {
  }

  CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION viewDimension,
                                 DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT mipSlice = 0,
                                 UINT firstArraySlice = 0, UINT arraySize = (UINT)-1,
                                 UINT flags = 0)
  {
    std::memset(this, 0, sizeof(*this));
    Format = format;
    ViewDimension = viewDimension;
    Flags = flags;

    switch (viewDimension)
    {
    case D3D11_DSV_DIMENSION_TEXTURE1D:
      Texture1D.MipSlice = mipSlice;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
      Texture1DArray.MipSlice = mipSlice;
      Texture1DArray.FirstArraySlice = firstArraySlice;
      Texture1DArray.ArraySize = arraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2D:
      Texture2D.MipSlice = mipSlice;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      Texture2DArray.MipSlice = mipSlice;
      Texture2DArray.FirstArraySlice = firstArraySlice;
      Texture2DArray.ArraySize = arraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS:
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      Texture2DMSArray.FirstArraySlice = firstArraySlice;
      Texture2DMSArray.ArraySize = arraySize;
      break;
    default:
      break;
    }
  }

  CD3D11_DEPTH_STENCIL_VIEW_DESC(ID3D11Texture2D* pTex2D, D3D11_DSV_DIMENSION viewDimension,
                                 DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT mipSlice = 0,
                                 UINT firstArraySlice = 0, UINT arraySize = (UINT)-1,
                                 UINT flags = 0)
  {
    std::memset(this, 0, sizeof(*this));
    ViewDimension = viewDimension;
    Flags = flags;

    D3D11_TEXTURE2D_DESC tex{};
    if (pTex2D)
      pTex2D->GetDesc(&tex);

    if (format == DXGI_FORMAT_UNKNOWN)
      format = tex.Format;
    if (arraySize == (UINT)-1 && (viewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY))
      arraySize = tex.ArraySize ? (tex.ArraySize - firstArraySlice) : 0;

    Format = format;

    switch (viewDimension)
    {
    case D3D11_DSV_DIMENSION_TEXTURE2D:
      Texture2D.MipSlice = mipSlice;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      Texture2DArray.MipSlice = mipSlice;
      Texture2DArray.FirstArraySlice = firstArraySlice;
      Texture2DArray.ArraySize = arraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS:
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      Texture2DMSArray.FirstArraySlice = firstArraySlice;
      Texture2DMSArray.ArraySize = arraySize;
      break;
    default:
      break;
    }
  }
};
#endif

namespace DX11
{
class DXTexture final : public AbstractTexture
{
public:
  ~DXTexture() override;

  static std::unique_ptr<DXTexture> Create(const TextureConfig& config, std::string_view name);
  static std::unique_ptr<DXTexture> CreateAdopted(ComPtr<ID3D11Texture2D> texture);

  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer, size_t buffer_size,
            u32 layer) override;

  ID3D11Texture2D* GetD3DTexture() const { return m_texture.Get(); }
  ID3D11ShaderResourceView* GetD3DSRV() const { return m_srv.Get(); }
  ID3D11UnorderedAccessView* GetD3DUAV() const { return m_uav.Get(); }

private:
  DXTexture(const TextureConfig& config, ComPtr<ID3D11Texture2D> texture, std::string_view name);

  bool CreateSRV();
  bool CreateUAV();

  ComPtr<ID3D11Texture2D> m_texture;
  ComPtr<ID3D11ShaderResourceView> m_srv;
  ComPtr<ID3D11UnorderedAccessView> m_uav;
  std::string m_name;
};

class DXStagingTexture final : public AbstractStagingTexture
{
public:
  DXStagingTexture() = delete;
  ~DXStagingTexture() override;

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
                       u32 src_layer, u32 src_level,
                       const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                     u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

  static std::unique_ptr<DXStagingTexture> Create(StagingTextureType type,
                                                  const TextureConfig& config);

private:
  DXStagingTexture(StagingTextureType type, const TextureConfig& config,
                   ComPtr<ID3D11Texture2D> tex);

  ComPtr<ID3D11Texture2D> m_tex = nullptr;
};

class DXFramebuffer final : public AbstractFramebuffer
{
public:
  DXFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                std::vector<AbstractTexture*> additional_color_attachments,
                AbstractTextureFormat color_format, AbstractTextureFormat depth_format, u32 width,
                u32 height, u32 layers, u32 samples, ComPtr<ID3D11RenderTargetView> rtv,
                ComPtr<ID3D11RenderTargetView> integer_rtv, ComPtr<ID3D11DepthStencilView> dsv,
                std::vector<ComPtr<ID3D11RenderTargetView>> additional_rtvs);
  ~DXFramebuffer() override;

  ID3D11RenderTargetView* const* GetRTVArray() const { return m_render_targets_raw.data(); }
  ID3D11RenderTargetView* const* GetIntegerRTVArray() const { return m_integer_rtv.GetAddressOf(); }
  UINT GetNumRTVs() const { return static_cast<UINT>(m_render_targets_raw.size()); }
  ID3D11DepthStencilView* GetDSV() const { return m_dsv.Get(); }

  void Unbind();
  void Clear(const ClearColor& color_value, float depth_value);

  static std::unique_ptr<DXFramebuffer>
  Create(DXTexture* color_attachment, DXTexture* depth_attachment,
         std::vector<AbstractTexture*> additional_color_attachments);

protected:
  std::vector<ComPtr<ID3D11RenderTargetView>> m_render_targets;
  std::vector<ID3D11RenderTargetView*> m_render_targets_raw;
  ComPtr<ID3D11RenderTargetView> m_integer_rtv;
  ComPtr<ID3D11DepthStencilView> m_dsv;
};

}  // namespace DX11
