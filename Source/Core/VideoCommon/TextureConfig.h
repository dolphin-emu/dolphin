// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

enum class AbstractTextureFormat : u32
{
  RGBA8,
  DXT1,
  DXT3,
  DXT5,
  BPTC
};

struct TextureConfig
{
  constexpr TextureConfig() = default;
  bool operator==(const TextureConfig& o) const;
  MathUtil::Rectangle<int> GetRect() const;

  u32 width = 0;
  u32 height = 0;
  u32 levels = 1;
  u32 layers = 1;
  AbstractTextureFormat format = AbstractTextureFormat::RGBA8;
  bool rendertarget = false;
};

namespace std
{
template <>
struct hash<TextureConfig>
{
  using argument_type = TextureConfig;
  using result_type = size_t;

  result_type operator()(const argument_type& c) const noexcept
  {
    const u64 id = static_cast<u64>(c.rendertarget) << 63 | static_cast<u64>(c.format) << 50 |
                   static_cast<u64>(c.layers) << 48 | static_cast<u64>(c.levels) << 32 |
                   static_cast<u64>(c.height) << 16 | static_cast<u64>(c.width);
    return std::hash<u64>{}(id);
  }
};
}  // namespace std
