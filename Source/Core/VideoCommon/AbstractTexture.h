// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture
{
public:
  explicit AbstractTexture(const TextureConfig& c);
  virtual ~AbstractTexture();

  virtual void CopyRectangleFromTexture(const AbstractTexture* src,
                                        const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                        u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                        u32 dst_layer, u32 dst_level) = 0;
  virtual void ScaleRectangleFromTexture(const AbstractTexture* source,
                                         const MathUtil::Rectangle<int>& srcrect,
                                         const MathUtil::Rectangle<int>& dstrect) = 0;
  virtual void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                    size_t buffer_size) = 0;

  bool Save(const std::string& filename, unsigned int level);

  static bool IsCompressedFormat(AbstractTextureFormat format);
  static size_t CalculateStrideForFormat(AbstractTextureFormat format, u32 row_length);
  static size_t GetTexelSizeForFormat(AbstractTextureFormat format);

  const TextureConfig& GetConfig() const;

protected:
  const TextureConfig m_config;
};
