// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureCacheBase.h"

namespace Null
{
class AbstractTexture : public AbstractTextureBase
{
public:
  AbstractTexture(const AbstractTextureBase::TextureConfig& _config) : AbstractTextureBase(_config)
  {
  }
  ~AbstractTexture() {}
  void Load(u8* data, unsigned int width, unsigned int height, unsigned int expanded_width,
            unsigned int level) override
  {
  }

  void FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
                        bool scale_by_half, unsigned int cbufid, const float* colmat) override
  {
  }

  void CopyRectangleFromTexture(const AbstractTextureBase* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override
  {
  }

  void Bind(unsigned int stage) override {}
  bool Save(const std::string& filename, unsigned int level) override { return false; }
};

class TextureCache : public TextureCacheBase
{
public:
  TextureCache() {}
  ~TextureCache() {}
  void CompileShaders() override {}
  void DeleteShaders() override {}
  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, void* palette,
                      TlutFormat format) override
  {
  }

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
               bool is_intensity, bool scale_by_half) override
  {
  }

  std::unique_ptr<AbstractTextureBase>
  CreateTexture(const AbstractTextureBase::TextureConfig& config) override
  {
    return std::make_unique<AbstractTexture>(config);
  }
};

}  // Null name space
