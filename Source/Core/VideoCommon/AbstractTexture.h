// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture
{
public:
  explicit AbstractTexture(const TextureConfig& c);
  virtual ~AbstractTexture();
  virtual void Bind(unsigned int stage) = 0;
  bool Save(const std::string& filename, unsigned int level);

  struct RawTextureInfo
  {
    const u8* data;
    u32 stride;
    u32 width;
    u32 height;
  };

  std::optional<RawTextureInfo> Map();
  std::optional<RawTextureInfo> Map(u32 level, u32 x, u32 y, u32 width, u32 height);
  std::optional<RawTextureInfo> Map(u32 level);
  virtual void Unmap();

  virtual void CopyRectangleFromTexture(const AbstractTexture* source,
                                        const MathUtil::Rectangle<int>& srcrect,
                                        const MathUtil::Rectangle<int>& dstrect) = 0;
  virtual void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                    size_t buffer_size) = 0;

  static bool IsCompressedHostTextureFormat(AbstractTextureFormat format);
  static size_t CalculateHostTextureLevelPitch(AbstractTextureFormat format, u32 row_length);

  const TextureConfig& GetConfig() const;

protected:
  virtual std::optional<RawTextureInfo> MapFullImpl();
  virtual std::optional<RawTextureInfo> MapRegionImpl(u32 level, u32 x, u32 y, u32 width,
                                                      u32 height);
  bool m_currently_mapped = false;

  const TextureConfig m_config;
};
