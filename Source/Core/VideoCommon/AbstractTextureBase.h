// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <tuple>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#include "VideoCommon/BPMemory.h"  // TODO: we shouldn't need to include this
#include "VideoCommon/VideoCommon.h"

class AbstractTextureBase
{
public:
  struct TextureConfig
  {
    constexpr TextureConfig() = default;

    bool operator==(const TextureConfig& o) const
    {
      return std::tie(width, height, levels, layers, rendertarget) ==
             std::tie(o.width, o.height, o.levels, o.layers, o.rendertarget);
    }

    struct Hasher : std::hash<u64>
    {
      size_t operator()(const TextureConfig& c) const
      {
        u64 id = (u64)c.rendertarget << 63 | (u64)c.layers << 48 | (u64)c.levels << 32 |
                 (u64)c.height << 16 | (u64)c.width;
        return std::hash<u64>::operator()(id);
      }
    };

    u32 width = 0;
    u32 height = 0;
    u32 levels = 1;
    u32 layers = 1;
    bool rendertarget = false;

    MathUtil::Rectangle<int> Rect() const
    {
      return {0, 0, static_cast<int>(width), static_cast<int>(height)};
    }
  };

  const TextureConfig config;

  AbstractTextureBase(const TextureConfig& c) : config(c) {}
  virtual ~AbstractTextureBase() {}
  virtual void Bind(unsigned int stage) = 0;
  virtual bool Save(const std::string& filename, unsigned int level) = 0;

  virtual void CopyRectangleFromTexture(const AbstractTextureBase* source,
                                        const MathUtil::Rectangle<int>& srcrect,
                                        const MathUtil::Rectangle<int>& dstrect) = 0;

  virtual void Load(u8* data, unsigned int width, unsigned int height, unsigned int expanded_width,
                    unsigned int level) = 0;
  virtual void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat,
                                const EFBRectangle& srcRect, bool scaleByHalf, unsigned int cbufid,
                                const float* colmat) = 0;
};