// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;

struct TextureAndTLUTFormat
{
  TextureAndTLUTFormat(TextureFormat texfmt_ = TextureFormat::I4,
                       TLUTFormat tlutfmt_ = TLUTFormat::IA8)
      : texfmt(texfmt_), tlutfmt(tlutfmt_)
  {
  }

  bool operator==(const TextureAndTLUTFormat& other) const
  {
    if (IsColorIndexed(texfmt))
      return texfmt == other.texfmt && tlutfmt == other.tlutfmt;

    return texfmt == other.texfmt;
  }

  bool operator!=(const TextureAndTLUTFormat& other) const { return !operator==(other); }
  TextureFormat texfmt;
  TLUTFormat tlutfmt;
};

struct EFBCopyParams
{
  EFBCopyParams(PEControl::PixelFormat efb_format_, EFBCopyFormat copy_format_, bool depth_,
                bool yuv_)
      : efb_format(efb_format_), copy_format(copy_format_), depth(depth_), yuv(yuv_)
  {
  }

  bool operator<(const EFBCopyParams& rhs) const
  {
    return std::tie(efb_format, copy_format, depth, yuv) <
           std::tie(rhs.efb_format, rhs.copy_format, rhs.depth, rhs.yuv);
  }

  PEControl::PixelFormat efb_format;
  EFBCopyFormat copy_format;
  bool depth;
  bool yuv;
};

class TextureCacheBase
{
private:
  static const int FRAMECOUNT_INVALID = 0;

public:
  struct TCacheEntry
  {
    // common members
    std::unique_ptr<AbstractTexture> texture;
    u32 addr;
    u32 size_in_bytes;
    u64 base_hash;
    u64 hash;  // for paletted textures, hash = base_hash ^ palette_hash
    TextureAndTLUTFormat format;
    u32 memory_stride;
    bool is_efb_copy;
    bool is_custom_tex;
    bool may_have_overlapping_textures = true;
    bool tmem_only = false;           // indicates that this texture only exists in the tmem cache
    bool has_arbitrary_mips = false;  // indicates that the mips in this texture are arbitrary
                                      // content, aren't just downscaled

    unsigned int native_width,
        native_height;  // Texture dimensions from the GameCube's point of view
    unsigned int native_levels;

    // used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
    int frameCount = FRAMECOUNT_INVALID;

    // Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when
    // removing the cache entry
    std::multimap<u64, TCacheEntry*>::iterator textures_by_hash_iter;

    // This is used to keep track of both:
    //   * efb copies used by this partially updated texture
    //   * partially updated textures which refer to this efb copy
    std::unordered_set<TCacheEntry*> references;

    explicit TCacheEntry(std::unique_ptr<AbstractTexture> tex);

    ~TCacheEntry();

    void SetGeneralParameters(u32 _addr, u32 _size, TextureAndTLUTFormat _format)
    {
      addr = _addr;
      size_in_bytes = _size;
      format = _format;
    }

    void SetDimensions(unsigned int _native_width, unsigned int _native_height,
                       unsigned int _native_levels)
    {
      native_width = _native_width;
      native_height = _native_height;
      native_levels = _native_levels;
      memory_stride = _native_width;
    }

    void SetHashes(u64 _base_hash, u64 _hash)
    {
      base_hash = _base_hash;
      hash = _hash;
    }

    // This texture entry is used by the other entry as a sub-texture
    void CreateReference(TCacheEntry* other_entry)
    {
      // References are two-way, so they can easily be destroyed later
      this->references.emplace(other_entry);
      other_entry->references.emplace(this);
    }

    void SetEfbCopy(u32 stride);

    bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

    bool IsEfbCopy() const { return is_efb_copy; }
    u32 NumBlocksY() const;
    u32 BytesPerRow() const;

    u64 CalculateHash() const;

    u32 GetWidth() const { return texture->GetConfig().width; }
    u32 GetHeight() const { return texture->GetConfig().height; }
    u32 GetNumLevels() const { return texture->GetConfig().levels; }
    u32 GetNumLayers() const { return texture->GetConfig().layers; }
    AbstractTextureFormat GetFormat() const { return texture->GetConfig().format; }
  };

  virtual ~TextureCacheBase();  // needs virtual for DX11 dtor

  void OnConfigChanged(VideoConfig& config);

  // Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
  // frameCount is the current frame number.
  void Cleanup(int _frameCount);

  void Invalidate();

  virtual void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
                       u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
                       bool scale_by_half) = 0;

  virtual bool CompileShaders() = 0;
  virtual void DeleteShaders() = 0;

  TCacheEntry* Load(const u32 stage);
  static void InvalidateAllBindPoints() { valid_bind_points.reset(); }
  static bool IsValidBindPoint(u32 i) { return valid_bind_points.test(i); }
  void BindTextures();
  void CopyRenderTargetToTexture(u32 dstAddr, EFBCopyFormat dstFormat, u32 dstStride,
                                 bool is_depth_copy, const EFBRectangle& gameSrcRect, const EFBRectangle& ourSrcRect, bool isIntensity,
                                 bool scaleByHalf);

  virtual void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, const void* palette,
                              TLUTFormat format) = 0;

  // Returns true if the texture data and palette formats are supported by the GPU decoder.
  virtual bool SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format)
  {
    return false;
  }

  // Decodes the specified data to the GPU texture specified by entry.
  // width, height are the size of the image in pixels.
  // aligned_width, aligned_height are the size of the image in pixels, aligned to the block size.
  // row_stride is the number of bytes for a row of blocks, not pixels.
  virtual void DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data,
                                  size_t data_size, TextureFormat format, u32 width, u32 height,
                                  u32 aligned_width, u32 aligned_height, u32 row_stride,
                                  const u8* palette, TLUTFormat palette_format)
  {
  }

protected:
  TextureCacheBase();

  GC_ALIGNED16(u8* temp);
  size_t temp_size;

  std::array<TCacheEntry*, 8> bound_textures{};
  static std::bitset<8> valid_bind_points;

private:
  // Minimal version of TCacheEntry just for TexPool
  struct TexPoolEntry
  {
    std::unique_ptr<AbstractTexture> texture;
    int frameCount = FRAMECOUNT_INVALID;
    TexPoolEntry(std::unique_ptr<AbstractTexture> tex) : texture(std::move(tex)) {}
  };
  using TexAddrCache = std::multimap<u32, TCacheEntry*>;
  using TexHashCache = std::multimap<u64, TCacheEntry*>;
  using TexPool = std::unordered_multimap<TextureConfig, TexPoolEntry>;

  void SetBackupConfig(const VideoConfig& config);

  TCacheEntry* ApplyPaletteToEntry(TCacheEntry* entry, u8* palette, TLUTFormat tlutfmt);

  void ScaleTextureCacheEntryTo(TCacheEntry* entry, u32 new_width, u32 new_height);
  TCacheEntry* DoPartialTextureUpdates(TCacheEntry* entry_to_update, u8* palette,
                                       TLUTFormat tlutfmt);

  void DumpTexture(TCacheEntry* entry, std::string basename, unsigned int level, bool is_arbitrary);
  void CheckTempSize(size_t required_size);

  TCacheEntry* AllocateCacheEntry(const TextureConfig& config);
  std::unique_ptr<AbstractTexture> AllocateTexture(const TextureConfig& config);
  TexPool::iterator FindMatchingTextureFromPool(const TextureConfig& config);
  TexAddrCache::iterator GetTexCacheIter(TCacheEntry* entry);

  // Return all possible overlapping textures. As addr+size of the textures is not
  // indexed, this may return false positives.
  std::pair<TexAddrCache::iterator, TexAddrCache::iterator>
  FindOverlappingTextures(u32 addr, u32 size_in_bytes);

  virtual std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) = 0;

  virtual void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                   const EFBRectangle& src_rect, bool scale_by_half,
                                   unsigned int cbuf_id, const float* colmat) = 0;

  // Removes and unlinks texture from texture cache and returns it to the pool
  TexAddrCache::iterator InvalidateTexture(TexAddrCache::iterator t_iter);

  TCacheEntry* ReturnEntry(unsigned int stage, TCacheEntry* entry);

  TexAddrCache textures_by_address;
  TexHashCache textures_by_hash;
  TexPool texture_pool;

  // Backup configuration values
  struct BackupConfig
  {
    int color_samples;
    bool texfmt_overlay;
    bool texfmt_overlay_center;
    bool hires_textures;
    bool cache_hires_textures;
    bool copy_cache_enable;
    bool stereo_3d;
    bool efb_mono_depth;
    bool gpu_texture_decoding;
  };
  BackupConfig backup_config = {};
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
