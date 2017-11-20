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

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureCacheEntry.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

class TextureCacheBackend;
struct VideoConfig;

struct TextureLookupInformation
{
  u32 address;

  u32 block_width;
  u32 block_height;
  u32 bytes_per_block;

  u32 expanded_width;
  u32 expanded_height;
  u32 native_width;
  u32 native_height;
  u32 total_bytes;
  u32 native_levels = 1;
  u32 computed_levels;

  u64 base_hash;
  u64 full_hash;

  TextureAndTLUTFormat full_format;
  u32 tlut_address = 0;

  bool is_palette_texture = false;
  u32 palette_size = 0;

  bool use_mipmaps = false;

  bool from_tmem = false;
  u32 tmem_address_even = 0;
  u32 tmem_address_odd = 0;

  int texture_cache_safety_color_sample_size = 0;  // Default to safe hashing

  u8* src_data;
};

class TextureCacheBase
{
public:
  explicit TextureCacheBase(std::unique_ptr<TextureCacheBackend> implementation);
  virtual ~TextureCacheBase();  // needs virtual for DX11 dtor

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

  TCacheEntry* GetXFBTexture(u32 address, u32 width, u32 height, TextureFormat texformat,
                             int textureCacheSafetyColorSampleSize);
  std::optional<TextureLookupInformation>
  ComputeTextureInformation(u32 address, u32 width, u32 height, TextureFormat texformat,
                            int textureCacheSafetyColorSampleSize, bool from_tmem,
                            u32 tmem_address_even, u32 tmem_address_odd, u32 tlutaddr,
                            TLUTFormat tlutfmt, u32 levels);
  TCacheEntry* GetXFBFromCache(const TextureLookupInformation& tex_info);
  bool LoadTextureFromOverlappingTextures(TCacheEntry* entry_to_update,
                                          const TextureLookupInformation& tex_info);
  TCacheEntry* CreateNormalTexture(const TextureLookupInformation& tex_info);
  void LoadTextureFromMemory(TCacheEntry* entry_to_update,
                             const TextureLookupInformation& tex_info);
  void LoadTextureLevelZeroFromMemory(TCacheEntry* entry_to_update,
                                      const TextureLookupInformation& tex_info, bool decode_on_gpu);
  virtual void BindTextures();
  void CopyRenderTargetToTexture(u32 dstAddr, EFBCopyFormat dstFormat, u32 dstStride,
                                 bool is_depth_copy, const EFBRectangle& srcRect, bool isIntensity,
                                 bool scaleByHalf, float y_scale, float gamma);

  void ScaleTextureCacheEntryTo(TCacheEntry* entry, u32 new_width, u32 new_height);

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config);

  TextureCacheBackend* GetBackendImpl() const;

protected:
  alignas(16) u8* temp = nullptr;
  size_t temp_size = 0;

  std::array<TCacheEntry*, 8> bound_textures{};
  static std::bitset<8> valid_bind_points;

private:
  // Minimal version of TCacheEntry just for TexPool
  struct TexPoolEntry
  {
    std::unique_ptr<AbstractTexture> texture;
    int frameCount = TCacheEntry::FRAMECOUNT_INVALID;
    TexPoolEntry(std::unique_ptr<AbstractTexture> tex) : texture(std::move(tex)) {}
  };
  using TexAddrCache = std::multimap<u32, TCacheEntry*>;
  using TexHashCache = std::multimap<u64, TCacheEntry*>;
  using TexPool = std::unordered_multimap<TextureConfig, TexPoolEntry>;

  void SetBackupConfig(const VideoConfig& config);

  TCacheEntry* ApplyPaletteToEntry(TCacheEntry* entry, u8* palette, TLUTFormat tlutfmt);

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

  // Removes and unlinks texture from texture cache and returns it to the pool
  TexAddrCache::iterator InvalidateTexture(TexAddrCache::iterator t_iter);

  void UninitializeXFBMemory(u8* dst, u32 stride, u32 bytes_per_row, u32 num_blocks_y);

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
  };
  BackupConfig backup_config = {};

  std::unique_ptr<TextureCacheBackend> backend_impl;
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
