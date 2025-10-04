// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

namespace Null
{
class NullTexture final : public AbstractTexture
{
public:
  explicit NullTexture(const TextureConfig& config);

  void CopyRectangleFromTexture(const AbstractTexture* src,
      const MathUtil::Rectangle<int>& src_rect, u32 src_layer, u32 src_level,
      const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer, u32 dst_level) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
      u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer, size_t buffer_size,
      u32 layer) override;
};

class NullStagingTexture final : public AbstractStagingTexture
{
public:
  explicit NullStagingTexture(StagingTextureType type, const TextureConfig& config);
  ~NullStagingTexture() override;

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
      u32 src_layer, u32 src_level, const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
      const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer, u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

private:
  std::vector<u8> m_texture_buf;
};

class NullFramebuffer final : public AbstractFramebuffer
{
public:
  explicit NullFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
      std::vector<AbstractTexture*> additional_color_attachments,
      AbstractTextureFormat color_format, AbstractTextureFormat depth_format, u32 width, u32 height,
      u32 layers, u32 samples);

  static std::unique_ptr<NullFramebuffer> Create(NullTexture* color_attachment,
      NullTexture* depth_attachment, std::vector<AbstractTexture*> additional_color_attachments);
};

}  // namespace Null
