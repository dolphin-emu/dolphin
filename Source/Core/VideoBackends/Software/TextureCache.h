// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/OGL/AbstractTexture.h"
#include "VideoBackends/Software/EfbCopy.h"
#include "VideoCommon/TextureCacheBase.h"

// Video cache doesn't need most of texture cache.
// But we use it to hook EFBCopy events and to get the XFB onto the screen via OpenGL

class TextureCache : public TextureCacheBase
{
public:
  void CompileShaders() override {};
  void DeleteShaders() override {};
  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, void* palette,
                      TlutFormat format) override {};
  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
               bool isIntensity, bool scaleByHalf) override
  {
    EfbCopy::CopyEfb();
  }

private:
  std::unique_ptr<AbstractTextureBase>
    CreateTexture(const AbstractTextureBase::TextureConfig& config) override
  {
    // We are just going to re-use the OpenGL backend's functionality.
    return std::make_unique<OGL::AbstractTexture>(config);
  }
};