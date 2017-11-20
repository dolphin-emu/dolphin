// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <tuple>

#include "Common/CommonTypes.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

class AbstractTexture;
struct TCacheEntry;
struct TextureConfig;

struct EFBCopyParams
{
  EFBCopyParams(PEControl::PixelFormat efb_format_, EFBCopyFormat copy_format_, bool depth_,
                bool yuv_, float y_scale_)
      : efb_format(efb_format_), copy_format(copy_format_), depth(depth_), yuv(yuv_),
        y_scale(y_scale_)
  {
  }

  bool operator<(const EFBCopyParams& rhs) const
  {
    return std::tie(efb_format, copy_format, depth, yuv, y_scale) <
           std::tie(rhs.efb_format, rhs.copy_format, rhs.depth, rhs.yuv, rhs.y_scale);
  }

  PEControl::PixelFormat efb_format;
  EFBCopyFormat copy_format;
  bool depth;
  bool yuv;
  float y_scale;
};

class TextureCacheBackend
{
public:
  virtual ~TextureCacheBackend() = default;
  virtual bool CompileShaders() = 0;
  virtual void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
                       u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
                       bool scale_by_half) = 0;
  virtual void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                   const EFBRectangle& src_rect, bool scale_by_half,
                                   unsigned int cbuf_id, const float* colmat) = 0;

  virtual void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, const void* palette,
                              TLUTFormat format) = 0;
  virtual std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) = 0;

  // Decodes the specified data to the GPU texture specified by entry.
  // width, height are the size of the image in pixels.
  // aligned_width, aligned_height are the size of the image in pixels, aligned to the block size.
  // row_stride is the number of bytes for a row of blocks, not pixels.
  virtual void DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data,
                                  size_t data_size, TextureFormat format, u32 width, u32 height,
                                  u32 aligned_width, u32 aligned_height, u32 row_stride,
                                  const u8* palette, TLUTFormat palette_format);

  virtual void DeleteShaders() = 0;

  // Returns true if the texture data and palette formats are supported by the GPU decoder.
  virtual bool SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format);
};
