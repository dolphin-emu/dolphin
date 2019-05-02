// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/AbstractTexture.h"

#include <tuple>

bool TextureConfig::operator==(const TextureConfig& o) const
{
  return std::tie(width, height, levels, layers, samples, format, flags) ==
         std::tie(o.width, o.height, o.levels, o.layers, o.samples, o.format, o.flags);
}

bool TextureConfig::operator!=(const TextureConfig& o) const
{
  return !operator==(o);
}

MathUtil::Rectangle<int> TextureConfig::GetRect() const
{
  return {0, 0, static_cast<int>(width), static_cast<int>(height)};
}

MathUtil::Rectangle<int> TextureConfig::GetMipRect(u32 level) const
{
  return {0, 0, static_cast<int>(std::max(width >> level, 1u)),
          static_cast<int>(std::max(height >> level, 1u))};
}

size_t TextureConfig::GetStride() const
{
  return AbstractTexture::CalculateStrideForFormat(format, width);
}

size_t TextureConfig::GetMipStride(u32 level) const
{
  return AbstractTexture::CalculateStrideForFormat(format, std::max(width >> level, 1u));
}
