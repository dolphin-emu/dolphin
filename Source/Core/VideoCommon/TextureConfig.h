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
  BGRA8,
  DXT1,
  DXT3,
  DXT5,
  BPTC,
  R16,
  D16,
  D24_S8,
  R32F,
  D32F,
  D32F_S8,
  Undefined
};

enum class StagingTextureType
{
  Readback,  // Optimize for CPU reads, GPU writes, no CPU writes
  Upload,    // Optimize for CPU writes, GPU reads, no CPU reads
  Mutable    // Optimize for CPU reads, GPU writes, allow slow CPU reads
};

struct TextureConfig
{
  constexpr TextureConfig() = default;
  constexpr TextureConfig(u32 width_, u32 height_, u32 levels_, u32 layers_, u32 samples_,
                          AbstractTextureFormat format_, bool rendertarget_)
      : width(width_), height(height_), levels(levels_), layers(layers_), samples(samples_),
        format(format_), rendertarget(rendertarget_)
  {
  }

  bool operator==(const TextureConfig& o) const;
  bool operator!=(const TextureConfig& o) const;
  MathUtil::Rectangle<int> GetRect() const;
  MathUtil::Rectangle<int> GetMipRect(u32 level) const;
  size_t GetStride() const;
  size_t GetMipStride(u32 level) const;
  bool IsMultisampled() const;

  u32 width = 0;
  u32 height = 0;
  u32 levels = 1;
  u32 layers = 1;
  u32 samples = 1;
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
