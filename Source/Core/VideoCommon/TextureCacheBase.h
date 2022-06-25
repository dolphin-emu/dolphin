// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <bitset>
#include <fmt/format.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureInfo.h"

class AbstractFramebuffer;
class AbstractStagingTexture;
class PointerWrap;
struct VideoConfig;

constexpr std::string_view EFB_DUMP_PREFIX = "efb1";
constexpr std::string_view XFB_DUMP_PREFIX = "xfb1";

namespace VideoCommon::TextureCache
{
struct CacheEntry;
}

struct EFBCopyParams
{
  EFBCopyParams(PixelFormat efb_format_, EFBCopyFormat copy_format_, bool depth_, bool yuv_,
                bool copy_filter_)
      : efb_format(efb_format_), copy_format(copy_format_), depth(depth_), yuv(yuv_),
        copy_filter(copy_filter_)
  {
  }

  bool operator<(const EFBCopyParams& rhs) const
  {
    return std::tie(efb_format, copy_format, depth, yuv, copy_filter) <
           std::tie(rhs.efb_format, rhs.copy_format, rhs.depth, rhs.yuv, rhs.copy_filter);
  }

  PixelFormat efb_format;
  EFBCopyFormat copy_format;
  bool depth;
  bool yuv;
  bool copy_filter;
};

template <>
struct fmt::formatter<EFBCopyParams>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const EFBCopyParams& uid, FormatContext& ctx) const
  {
    std::string copy_format;
    if (uid.copy_format == EFBCopyFormat::XFB)
      copy_format = "XFB";
    else
      copy_format = fmt::to_string(uid.copy_format);
    return fmt::format_to(ctx.out(),
                          "format: {}, copy format: {}, depth: {}, yuv: {}, copy filter: {}",
                          uid.efb_format, copy_format, uid.depth, uid.yuv, uid.copy_filter);
  }
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
  using TCacheEntry = VideoCommon::TextureCache::CacheEntry;

  // Minimal version of TCacheEntry just for TexPool
  struct TexPoolEntry
  {
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    int frameCount = FRAMECOUNT_INVALID;

    TexPoolEntry(std::unique_ptr<AbstractTexture> tex, std::unique_ptr<AbstractFramebuffer> fb);
  };

  TextureCacheBase();
  virtual ~TextureCacheBase();

  bool Initialize();

  void OnConfigChanged(const VideoConfig& config);
  void ForceReload();

  // Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
  // frameCount is the current frame number.
  void Cleanup(int _frameCount);

  void Invalidate();

  TCacheEntry* Load(const TextureInfo& texture_info);
  TCacheEntry* GetTexture(const int textureCacheSafetyColorSampleSize,
                          const TextureInfo& texture_info);
  TCacheEntry* GetXFBTexture(u32 address, u32 width, u32 height, u32 stride,
                             MathUtil::Rectangle<int>* display_rect);

  virtual void BindTextures(BitSet32 used_textures);
  void CopyRenderTargetToTexture(u32 dstAddr, EFBCopyFormat dstFormat, u32 width, u32 height,
                                 u32 dstStride, bool is_depth_copy,
                                 const MathUtil::Rectangle<int>& srcRect, bool isIntensity,
                                 bool scaleByHalf, float y_scale, float gamma, bool clamp_top,
                                 bool clamp_bottom,
                                 const CopyFilterCoefficients::Values& filter_coefficients);

  void ScaleTextureCacheEntryTo(TCacheEntry* entry, u32 new_width, u32 new_height);

  // Flushes all pending EFB copies to emulated RAM.
  void FlushEFBCopies();

  // Texture Serialization
  void SerializeTexture(AbstractTexture* tex, const TextureConfig& config, PointerWrap& p);
  std::optional<TexPoolEntry> DeserializeTexture(PointerWrap& p);

  // Save States
  void DoState(PointerWrap& p);

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
  using TexAddrCache = std::multimap<u32, TCacheEntry*>;
  using TexHashCache = std::multimap<u64, TCacheEntry*>;
  using TexPool = std::unordered_multimap<TextureConfig, TexPoolEntry>;

  bool CreateUtilityTextures();

  void SetBackupConfig(const VideoConfig& config);

  TCacheEntry* GetXFBFromCache(u32 address, u32 width, u32 height, u32 stride);

  TCacheEntry* ApplyPaletteToEntry(TCacheEntry* entry, const u8* palette, TLUTFormat tlutfmt);

  TCacheEntry* ReinterpretEntry(const TCacheEntry* existing_entry, TextureFormat new_format);

  TCacheEntry* DoPartialTextureUpdates(TCacheEntry* entry_to_update, const u8* palette,
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

  void UninitializeEFBMemory(u8* dst, u32 stride, u32 bytes_per_row, u32 num_blocks_y);
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

  bool CheckReadbackTexture(u32 width, u32 height, AbstractTextureFormat format);
  void DoSaveState(PointerWrap& p);
  void DoLoadState(PointerWrap& p);

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
    bool copy_cache_enable;
    bool stereo_3d;
    bool efb_mono_depth;
    bool gpu_texture_decoding;
    bool disable_vram_copies;
    bool arbitrary_mipmap_detection;
    bool graphics_mods;
    u32 graphics_mod_change_count;
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

  // Staging texture used for readbacks.
  // We store this in the class so that the same staging texture can be used for multiple
  // readbacks, saving the overhead of allocating a new buffer every time.
  std::unique_ptr<AbstractStagingTexture> m_readback_texture;
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
