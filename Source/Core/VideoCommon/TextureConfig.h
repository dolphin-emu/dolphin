// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

// Format of a texture on the host. All backends support RGBA8.
enum class AbstractTextureFormat : u32
{
  RGBA8,   // R G B A R G B A ...
  I8,      // I I I I ...
  AI4,     // T T T T ... where T = (A << 4) | I
  AI8,     // A I A I ...
  RGB565,  // T T T T ... where T = (R << 11) | (G << 5) | B
  ARGB4,   // T T T T ... where T = (A << 12) | (R << 8) | (B << 4) | B
  DXT1,
  DXT3,
  DXT5,
  BPTC,
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

  struct Hasher : std::hash<u64>
  {
    size_t operator()(const TextureConfig& c) const
    {
      u64 id = (u64)c.rendertarget << 63 | (u64)c.format << 50 | (u64)c.layers << 48 |
               (u64)c.levels << 32 | (u64)c.height << 16 | (u64)c.width;
      return std::hash<u64>::operator()(id);
    }
  };
};
