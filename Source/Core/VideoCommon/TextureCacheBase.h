// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureDecoder.h"

class AbstractFramebuffer;
class AbstractStagingTexture;
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
                bool yuv_, bool copy_filter_)
      : efb_format(efb_format_), copy_format(copy_format_), depth(depth_), yuv(yuv_),
        copy_filter(copy_filter_)
  {
  }

  bool operator<(const EFBCopyParams& rhs) const
  {
    return std::tie(efb_format, copy_format, depth, yuv, copy_filter) <
           std::tie(rhs.efb_format, rhs.copy_format, rhs.depth, rhs.yuv, rhs.copy_filter);
  }

  PEControl::PixelFormat efb_format;
  EFBCopyFormat copy_format;
  bool depth;
  bool yuv;
  bool copy_filter;
};

// Reduced version of the full coefficient array, with a single value for each row.
struct EFBCopyFilterCoefficients
{
  float upper;
  float middle;
  float lower;
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
    std::unique_ptr<AbstractFramebuffer> framebuffer;
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
    bool should_force_safe_hashing = false;  // for XFB
    bool is_xfb_copy = false;
    bool is_xfb_container = false;
    u64 id;

    bool reference_changed = false;  // used by xfb to determine when a reference xfb changed

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

    // Pending EFB copy
    std::unique_ptr<AbstractStagingTexture> pending_efb_copy;
    u32 pending_efb_copy_width = 0;
    u32 pending_efb_copy_height = 0;
    bool pending_efb_copy_invalidated = false;

    explicit TCacheEntry(std::unique_ptr<AbstractTexture> tex,
                         std::unique_ptr<AbstractFramebuffer> fb);

    ~TCacheEntry();

    void SetGeneralParameters(u32 _addr, u32 _size, TextureAndTLUTFormat _format,
                              bool force_safe_hashing)
    {
      addr = _addr;
      size_in_bytes = _size;
      format = _format;
      should_force_safe_hashing = force_safe_hashing;
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

    void SetXfbCopy(u32 stride);
    void SetEfbCopy(u32 stride);
    void SetNotCopy();

    bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

    bool IsEfbCopy() const { return is_efb_copy; }
    bool IsCopy() const { return is_xfb_copy || is_efb_copy; }
    u32 NumBlocksY() const;
    u32 BytesPerRow() const;

    u64 CalculateHash() const;

    int HashSampleSize() const;
    u32 GetWidth() const { return texture->GetConfig().width; }
    u32 GetHeight() const { return texture->GetConfig().height; }
    u32 GetNumLevels() const { return texture->GetConfig().levels; }
    u32 GetNumLayers() const { return texture->GetConfig().layers; }
    AbstractTextureFormat GetFormat() const { return texture->GetConfig().format; }
  };

  TextureCacheBase();
  virtual ~TextureCacheBase();

  bool Initialize();

  void OnConfigChanged(VideoConfig& config);

  // Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
  // frameCount is the current frame number.
  void Cleanup(int _frameCount);

  void Invalidate();

  TCacheEntry* Load(const u32 stage);
  static void InvalidateAllBindPoints() { valid_bind_points.reset(); }
  static bool IsValidBindPoint(u32 i) { return valid_bind_points.test(i); }
  TCacheEntry* GetTexture(u32 address, u32 width, u32 height, const TextureFormat texformat,
                          const int textureCacheSafetyColorSampleSize, u32 tlutaddr = 0,
                          TLUTFormat tlutfmt = TLUTFormat::IA8, bool use_mipmaps = false,
                          u32 tex_levels = 1, bool from_tmem = false, u32 tmem_address_even = 0,
                          u32 tmem_address_odd = 0);
  TCacheEntry* GetXFBTexture(u32 address, u32 width, u32 height, u32 stride,
                             MathUtil::Rectangle<int>* display_rect);

  virtual void BindTextures();
  void CopyRenderTargetToTexture(u32 dstAddr, EFBCopyFormat dstFormat, u32 width, u32 height,
                                 u32 dstStride, bool is_depth_copy,
                                 const MathUtil::Rectangle<int>& srcRect, bool isIntensity,
                                 bool scaleByHalf, float y_scale, float gamma, bool clamp_top,
                                 bool clamp_bottom,
                                 const CopyFilterCoefficients::Values& filter_coefficients);

  void ScaleTextureCacheEntryTo(TCacheEntry* entry, u32 new_width, u32 new_height);

  // Flushes all pending EFB copies to emulated RAM.
  void FlushEFBCopies();

  // Returns false if the top/bottom row coefficients are zero.
  static bool NeedsCopyFilterInShader(const EFBCopyFilterCoefficients& coefficients);

protected:
  // Decodes the specified data to the GPU texture specified by entry.
  // Returns false if the configuration is not supported.
  // width, height are the size of the image in pixels.
  // aligned_width, aligned_height are the size of the image in pixels, aligned to the block size.
  // row_stride is the number of bytes for a row of blocks, not pixels.
  bool DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data, u32 data_size,
                          TextureFormat format, u32 width, u32 height, u32 aligned_width,
                          u32 aligned_height, u32 row_stride, const u8* palette,
                          TLUTFormat palette_format);

  virtual void CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params, u32 native_width,
                       u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                       const MathUtil::Rectangle<int>& src_rect, bool scale_by_half,
                       bool linear_filter, float y_scale, float gamma, bool clamp_top,
                       bool clamp_bottom, const EFBCopyFilterCoefficients& filter_coefficients);
  virtual void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                   const MathUtil::Rectangle<int>& src_rect, bool scale_by_half,
                                   bool linear_filter, EFBCopyFormat dst_format, bool is_intensity,
                                   float gamma, bool clamp_top, bool clamp_bottom,
                                   const EFBCopyFilterCoefficients& filter_coefficients);

  alignas(16) u8* temp = nullptr;
  size_t temp_size = 0;

  std::array<TCacheEntry*, 8> bound_textures{};
  static std::bitset<8> valid_bind_points;

private:
  // Minimal version of TCacheEntry just for TexPool
  struct TexPoolEntry
  {
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    int frameCount = FRAMECOUNT_INVALID;

    TexPoolEntry(std::unique_ptr<AbstractTexture> tex, std::unique_ptr<AbstractFramebuffer> fb);
  };
  using TexAddrCache = std::multimap<u32, TCacheEntry*>;
  using TexHashCache = std::multimap<u64, TCacheEntry*>;
  using TexPool = std::unordered_multimap<TextureConfig, TexPoolEntry>;

  bool CreateUtilityTextures();

  void SetBackupConfig(const VideoConfig& config);

  TCacheEntry* GetXFBFromCache(u32 address, u32 width, u32 height, u32 stride, u64 hash);

  TCacheEntry* ApplyPaletteToEntry(TCacheEntry* entry, u8* palette, TLUTFormat tlutfmt);

  TCacheEntry* ReinterpretEntry(const TCacheEntry* existing_entry, TextureFormat new_format);

  TCacheEntry* DoPartialTextureUpdates(TCacheEntry* entry_to_update, u8* palette,
                                       TLUTFormat tlutfmt);
  void StitchXFBCopy(TCacheEntry* entry_to_update);

  void DumpTexture(TCacheEntry* entry, std::string basename, unsigned int level, bool is_arbitrary);
  void CheckTempSize(size_t required_size);

  TCacheEntry* AllocateCacheEntry(const TextureConfig& config);
  std::optional<TexPoolEntry> AllocateTexture(const TextureConfig& config);
  TexPool::iterator FindMatchingTextureFromPool(const TextureConfig& config);
  TexAddrCache::iterator GetTexCacheIter(TCacheEntry* entry);

  // Return all possible overlapping textures. As addr+size of the textures is not
  // indexed, this may return false positives.
  std::pair<TexAddrCache::iterator, TexAddrCache::iterator>
  FindOverlappingTextures(u32 addr, u32 size_in_bytes);

  // Removes and unlinks texture from texture cache and returns it to the pool
  TexAddrCache::iterator InvalidateTexture(TexAddrCache::iterator t_iter,
                                           bool discard_pending_efb_copy = false);

  void UninitializeXFBMemory(u8* dst, u32 stride, u32 bytes_per_row, u32 num_blocks_y);

  // Precomputing the coefficients for the previous, current, and next lines for the copy filter.
  static EFBCopyFilterCoefficients
  GetRAMCopyFilterCoefficients(const CopyFilterCoefficients::Values& coefficients);
  static EFBCopyFilterCoefficients
  GetVRAMCopyFilterCoefficients(const CopyFilterCoefficients::Values& coefficients);

  // Flushes a pending EFB copy to RAM from the host to the guest RAM.
  void WriteEFBCopyToRAM(u8* dst_ptr, u32 width, u32 height, u32 stride,
                         std::unique_ptr<AbstractStagingTexture> staging_texture);
  void FlushEFBCopy(TCacheEntry* entry);

  // Returns a staging texture of the maximum EFB copy size.
  std::unique_ptr<AbstractStagingTexture> GetEFBCopyStagingTexture();

  // Returns an EFB copy staging texture to the pool, so it can be re-used.
  void ReleaseEFBCopyStagingTexture(std::unique_ptr<AbstractStagingTexture> tex);

  TexAddrCache textures_by_address;
  TexHashCache textures_by_hash;
  TexPool texture_pool;
  u64 last_entry_id = 0;

  // Backup configuration values
  struct BackupConfig
  {
    int color_samples;
    bool texfmt_overlay;
    bool texfmt_overlay_center;
    bool hires_textures;
    bool cache_hires_textures;
    bool gpu_texture_decoding;
    bool disable_vram_copies;
    bool arbitrary_mipmap_detection;
    bool tmem_cache_emulation;
  };
  BackupConfig backup_config = {};

  // Encoding texture used for EFB copies to RAM.
  std::unique_ptr<AbstractTexture> m_efb_encoding_texture;
  std::unique_ptr<AbstractFramebuffer> m_efb_encoding_framebuffer;

  // Decoding texture used for GPU texture decoding.
  std::unique_ptr<AbstractTexture> m_decoding_texture;

  // Pool of readback textures used for deferred EFB copies.
  std::vector<std::unique_ptr<AbstractStagingTexture>> m_efb_copy_staging_texture_pool;

  // List of pending EFB copies. It is important that the order is preserved for these,
  // so that overlapping textures are written to guest RAM in the order they are issued.
  std::vector<TCacheEntry*> m_pending_efb_copies;
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
