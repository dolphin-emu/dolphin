// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;

class TextureCacheBase
{
public:
  struct TCacheEntryConfig
  {
    constexpr TCacheEntryConfig() = default;

    bool operator==(const TCacheEntryConfig& o) const
    {
      return std::tie(width, height, levels, layers, rendertarget) ==
             std::tie(o.width, o.height, o.levels, o.layers, o.rendertarget);
    }

    struct Hasher : std::hash<u64>
    {
      size_t operator()(const TCacheEntryConfig& c) const
      {
        u64 id = (u64)c.rendertarget << 63 | (u64)c.layers << 48 | (u64)c.levels << 32 |
                 (u64)c.height << 16 | (u64)c.width;
        return std::hash<u64>::operator()(id);
      }
    };

    u32 width = 0;
    u32 height = 0;
    u32 levels = 1;
    u32 layers = 1;
    bool rendertarget = false;
  };

  struct TCacheEntryBase
  {
    const TCacheEntryConfig config;

    // common members
    u32 addr;
    u32 size_in_bytes;
    u64 base_hash;
    u64 hash;    // for paletted textures, hash = base_hash ^ palette_hash
    u32 format;  // bits 0-3 will contain the in-memory format.
    u32 memory_stride;
    bool is_efb_copy;
    bool is_custom_tex;
    bool may_have_overlapping_textures;

    unsigned int native_width,
        native_height;  // Texture dimensions from the GameCube's point of view
    unsigned int native_levels;

    // used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
    int frameCount;

    // Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when
    // removing the cache entry
    std::multimap<u64, TCacheEntryBase*>::iterator textures_by_hash_iter;

    // This is used to keep track of both:
    //   * efb copies used by this partially updated texture
    //   * partially updated textures which refer to this efb copy
    std::unordered_set<TCacheEntryBase*> references;

    void SetGeneralParameters(u32 _addr, u32 _size, u32 _format)
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
    void CreateReference(TCacheEntryBase* other_entry)
    {
      // References are two-way, so they can easily be destroyed later
      this->references.emplace(other_entry);
      other_entry->references.emplace(this);
    }

    void DestroyAllReferences()
    {
      for (auto& reference : references)
        reference->references.erase(this);

      references.clear();
    }

    void SetEfbCopy(u32 stride);

    TCacheEntryBase(const TCacheEntryConfig& c) : config(c) {}
    virtual ~TCacheEntryBase();

    virtual void Bind(unsigned int stage) = 0;
    virtual bool Save(const std::string& filename, unsigned int level) = 0;

    virtual void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                          const MathUtil::Rectangle<int>& srcrect,
                                          const MathUtil::Rectangle<int>& dstrect) = 0;

    virtual void Load(const u8* buffer, u32 width, u32 height, u32 expanded_width, u32 level) = 0;
    virtual void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect, bool scaleByHalf,
                                  unsigned int cbufid, const float* colmat) = 0;

    bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

    bool IsEfbCopy() const { return is_efb_copy; }
    u32 NumBlocksY() const;
    u32 BytesPerRow() const;

    u64 CalculateHash() const;
  };

  virtual ~TextureCacheBase();  // needs virtual for DX11 dtor

  void OnConfigChanged(VideoConfig& config);

  // Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
  // frameCount is the current frame number.
  void Cleanup(int _frameCount);

  void Invalidate();

  virtual TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) = 0;

  virtual void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
                       u32 memory_stride, bool is_depth_copy, const EFBRectangle& srcRect,
                       bool isIntensity, bool scaleByHalf) = 0;

  virtual bool CompileShaders() = 0;
  virtual void DeleteShaders() = 0;

  TCacheEntryBase* Load(const u32 stage);
  void UnbindTextures();
  virtual void BindTextures();
  void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, u32 dstStride,
                                 bool is_depth_copy, const EFBRectangle& srcRect, bool isIntensity,
                                 bool scaleByHalf);

  virtual void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette,
                              TlutFormat format) = 0;

protected:
  TextureCacheBase();

  alignas(16) u8* temp = nullptr;
  size_t temp_size = 0;

  std::array<TCacheEntryBase*, 8> bound_textures{};

private:
  typedef std::multimap<u32, TCacheEntryBase*> TexAddrCache;
  typedef std::multimap<u64, TCacheEntryBase*> TexHashCache;
  typedef std::unordered_multimap<TCacheEntryConfig, TCacheEntryBase*, TCacheEntryConfig::Hasher>
      TexPool;

  void SetBackupConfig(const VideoConfig& config);

  TCacheEntryBase* ApplyPaletteToEntry(TCacheEntryBase* entry, u8* palette, u32 tlutfmt);

  void ScaleTextureCacheEntryTo(TCacheEntryBase** entry, u32 new_width, u32 new_height);
  TCacheEntryBase* DoPartialTextureUpdates(TCacheEntryBase* entry_to_update, u8* palette,
                                           u32 tlutfmt);

  void DumpTexture(TCacheEntryBase* entry, std::string basename, unsigned int level);
  void CheckTempSize(size_t required_size);

  TCacheEntryBase* AllocateTexture(const TCacheEntryConfig& config);
  TexPool::iterator FindMatchingTextureFromPool(const TCacheEntryConfig& config);
  TexAddrCache::iterator GetTexCacheIter(TCacheEntryBase* entry);

  // Return all possible overlapping textures. As addr+size of the textures is not
  // indexed, this may return false positives.
  std::pair<TexAddrCache::iterator, TexAddrCache::iterator>
  FindOverlappingTextures(u32 addr, u32 size_in_bytes);

  // Removes and unlinks texture from texture cache and returns it to the pool
  TexAddrCache::iterator InvalidateTexture(TexAddrCache::iterator t_iter);

  TCacheEntryBase* ReturnEntry(unsigned int stage, TCacheEntryBase* entry);

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
  };
  BackupConfig backup_config = {};
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
