// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <memory>
#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

namespace DX11
{
class DXTexture final : public AbstractTexture
{
public:
  explicit DXTexture(const TextureConfig& tex_config, ID3D11Texture2D* d3d_texture,
                     ID3D11ShaderResourceView* d3d_srv, ID3D11UnorderedAccessView* d3d_uav);
  ~DXTexture();

  static std::unique_ptr<DXTexture> Create(const TextureConfig& config);

  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

  ID3D11Texture2D* GetD3DTexture() const { return m_d3d_texture; }
  ID3D11ShaderResourceView* GetD3DSRV() const { return m_d3d_srv; }
  ID3D11UnorderedAccessView* GetD3DUAV() const { return m_d3d_uav; }

private:
  ID3D11Texture2D* m_d3d_texture;
  ID3D11ShaderResourceView* m_d3d_srv;
  ID3D11UnorderedAccessView* m_d3d_uav;
};

class DXStagingTexture final : public AbstractStagingTexture
{
public:
  DXStagingTexture() = delete;
  ~DXStagingTexture();

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
  DXStagingTexture(StagingTextureType type, const TextureConfig& config, ID3D11Texture2D* tex);

  ID3D11Texture2D* m_tex = nullptr;
};

class DXFramebuffer final : public AbstractFramebuffer
{
public:
  DXFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                AbstractTextureFormat color_format, AbstractTextureFormat depth_format, u32 width,
                u32 height, u32 layers, u32 samples, ID3D11RenderTargetView* rtv,
                ID3D11RenderTargetView* integer_rtv, ID3D11DepthStencilView* dsv);
  ~DXFramebuffer() override;

  ID3D11RenderTargetView* const* GetRTVArray() const { return &m_rtv; }
  ID3D11RenderTargetView* const* GetIntegerRTVArray() const { return &m_integer_rtv; }
  UINT GetNumRTVs() const { return m_rtv ? 1 : 0; }
  ID3D11DepthStencilView* GetDSV() const { return m_dsv; }
  static std::unique_ptr<DXFramebuffer> Create(DXTexture* color_attachment,
                                               DXTexture* depth_attachment);

protected:
  ID3D11RenderTargetView* m_rtv;
  ID3D11RenderTargetView* m_integer_rtv;
  ID3D11DepthStencilView* m_dsv;
};

}  // namespace DX11
