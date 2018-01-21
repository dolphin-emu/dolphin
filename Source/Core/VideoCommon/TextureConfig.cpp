// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/TextureConfig.h"

#include <tuple>

bool TextureConfig::operator==(const TextureConfig& o) const
{
  return std::tie(width, height, levels, layers, format, rendertarget) ==
         std::tie(o.width, o.height, o.levels, o.layers, o.format, o.rendertarget);
}

MathUtil::Rectangle<int> TextureConfig::GetRect() const
{
  return {0, 0, static_cast<int>(width), static_cast<int>(height)};
}
