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
