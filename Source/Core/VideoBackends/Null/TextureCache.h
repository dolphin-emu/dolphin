// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureCacheBase.h"

namespace Null
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache() {}
  ~TextureCache() {}
  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette,
                      TlutFormat format) override
  {
  }

  void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
               const EFBRectangle& src_rect, bool scale_by_half) override
  {
  }

private:
  struct TCacheEntry : TCacheEntryBase
  {
    TCacheEntry(const TCacheEntryConfig& _config) : TCacheEntryBase(_config) {}
    ~TCacheEntry() {}
    void Load(const u8* buffer, u32 width, u32 height, u32 expanded_width, u32 level) override {}
    void FromRenderTarget(bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half,
                          unsigned int cbufid, const float* colmat) override
    {
    }

    void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                  const MathUtil::Rectangle<int>& srcrect,
                                  const MathUtil::Rectangle<int>& dstrect) override
    {
    }

    void Bind(unsigned int stage) override {}
    bool Save(const std::string& filename, unsigned int level) override { return false; }
  };

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override
  {
    return new TCacheEntry(config);
  }
};

}  // Null name space
