// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Null/NullTexture.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConfig.h"

namespace Null
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache() {}
  ~TextureCache() {}
  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, const void* palette,
                      TLUTFormat format) override
  {
  }

  void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half) override
  {
  }

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, unsigned int cbuf_id, const float* colmat) override
  {
  }

private:
  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override
  {
    return std::make_unique<NullTexture>(config);
  }
};

}  // Null name space
