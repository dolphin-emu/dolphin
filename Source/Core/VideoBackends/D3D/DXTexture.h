// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

class D3DTexture2D;

namespace DX11
{
class DXTexture final : public AbstractTexture
{
public:
  explicit DXTexture(const TextureConfig& tex_config);
  ~DXTexture();

  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ScaleRectangleFromTexture(const AbstractTexture* source,
                                 const MathUtil::Rectangle<int>& srcrect,
                                 const MathUtil::Rectangle<int>& dstrect) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

  D3DTexture2D* GetRawTexIdentifier() const;

private:
  D3DTexture2D* m_texture;
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
  DXFramebuffer(AbstractTextureFormat color_format, AbstractTextureFormat depth_format, u32 width,
                u32 height, u32 layers, u32 samples, ID3D11RenderTargetView* rtv,
                ID3D11DepthStencilView* dsv);
  ~DXFramebuffer() override;

  ID3D11RenderTargetView* const* GetRTVArray() const { return &m_rtv; }
  UINT GetNumRTVs() const { return m_rtv ? 1 : 0; }
  ID3D11DepthStencilView* GetDSV() const { return m_dsv; }
  static std::unique_ptr<DXFramebuffer> Create(const DXTexture* color_attachment,
                                               const DXTexture* depth_attachment);

protected:
  ID3D11RenderTargetView* m_rtv;
  ID3D11DepthStencilView* m_dsv;
};

}  // namespace DX11
