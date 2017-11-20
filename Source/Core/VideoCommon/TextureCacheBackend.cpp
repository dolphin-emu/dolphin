// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/TextureCacheBackend.h"
#include "VideoCommon/TextureCacheEntry.h"

void TextureCacheBackend::DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data,
                                             size_t data_size, TextureFormat format, u32 width,
                                             u32 height, u32 aligned_width, u32 aligned_height,
                                             u32 row_stride, const u8* palette,
                                             TLUTFormat palette_format)
{
}

bool TextureCacheBackend::SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format)
{
  return false;
}
