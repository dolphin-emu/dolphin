// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.h>

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

#include "VideoBackends/Metal/MRCHelpers.h"

namespace Metal
{
class Texture final : public AbstractTexture
{
public:
  explicit Texture(MRCOwned<id<MTLTexture>> tex, const TextureConfig& config);
  ~Texture();

  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer, size_t buffer_size,
            u32 layer) override;

  id<MTLTexture> GetMTLTexture() const { return m_tex; }
  void SetMTLTexture(MRCOwned<id<MTLTexture>> tex) { m_tex = std::move(tex); }

private:
  MRCOwned<id<MTLTexture>> m_tex;
};

class StagingTexture final : public AbstractStagingTexture
{
public:
  StagingTexture(MRCOwned<id<MTLBuffer>> buffer, StagingTextureType type,
                 const TextureConfig& config);
  ~StagingTexture();

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
                       u32 src_layer, u32 src_level,
                       const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                     u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

private:
  MRCOwned<id<MTLBuffer>> m_buffer;
  MRCOwned<id<MTLCommandBuffer>> m_wait_buffer;
};

class Framebuffer final : public AbstractFramebuffer
{
public:
  Framebuffer(AbstractTexture* color, AbstractTexture* depth, u32 width, u32 height, u32 layers,
              u32 samples);
  ~Framebuffer();

  id<MTLTexture> GetColor() const
  {
    return static_cast<Texture*>(GetColorAttachment())->GetMTLTexture();
  }
  id<MTLTexture> GetDepth() const
  {
    return static_cast<Texture*>(GetDepthAttachment())->GetMTLTexture();
  }
};
}  // namespace Metal
