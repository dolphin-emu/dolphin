// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTextureBase.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;

class TextureCacheBase
{
public:
  // struct TCacheEntry;

private:
  static const int FRAMECOUNT_INVALID = 0;
  // typedef std::multimap<u64, TCacheEntry*> TexCache;

public:
  struct TCacheEntry
  {
    // common members
    std::unique_ptr<AbstractTextureBase> texture;
    u32 addr;
    u32 size_in_bytes;
    u64 base_hash;
    u64 hash;    // for paletted textures, hash = base_hash ^ palette_hash
    u32 format;  // bits 0-3 will contain the in-memory format.
    bool is_efb_copy;
    bool is_custom_tex;
    u32 memory_stride;

    unsigned int native_width,
        native_height;  // Texture dimensions from the GameCube's point of view
    unsigned int native_levels;

    // used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
    int frameCount = FRAMECOUNT_INVALID;

    // Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when
    // removing the cache entry
    std::multimap<u64, TCacheEntry*>::iterator textures_by_hash_iter = textures_by_hash.end();

    // This is used to keep track of both:
    //   * efb copies used by this partially updated texture
    //   * partially updated textures which refer to this efb copy
    std::unordered_set<TCacheEntry*> references;

    TCacheEntry(std::unique_ptr<AbstractTextureBase> texture) : texture(std::move(texture)) {}
    ~TCacheEntry()
    {
      // Destroy All References
      for (auto& reference : references)
        reference->references.erase(this);
    }

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
    void CreateReference(TCacheEntry* other_entry)
    {
      // References are two-way, so they can easily be destroyed later
      this->references.emplace(other_entry);
      other_entry->references.emplace(this);
    }

    void SetEfbCopy(u32 stride);

    bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

    TextureCacheBase::TCacheEntry* ApplyPalette(u8* palette, u32 tlutfmt);

    bool IsEfbCopy() const { return is_efb_copy; }
    u32 NumBlocksY() const;
    u32 BytesPerRow() const;

    u64 CalculateHash() const;

    u32 width() const { return texture->config.width; }
    u32 height() const { return texture->config.height; }
    u32 levels() const { return texture->config.levels; }
    u32 layers() const { return texture->config.layers; }
  };

  virtual ~TextureCacheBase();  // needs virtual for DX11 dtor

  static void OnConfigChanged(VideoConfig& config);

  // Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
  // frameCount is the current frame number.
  static void Cleanup(int _frameCount);

  static void Invalidate();

  virtual void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
                       u32 memory_stride, PEControl::PixelFormat srcFormat,
                       const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf) = 0;

  virtual void CompileShaders() = 0;  // currently only implemented by OGL
  virtual void DeleteShaders() = 0;   // currently only implemented by OGL

  static TCacheEntry* Load(const u32 stage);
  static TextureCacheBase::TCacheEntry*
  GetTexture(u32 address, u32 width, u32 height, const int texformat, u32 tlutaddr = 0,
             u32 tlutfmt = 0, bool use_mipmaps = false, u32 tex_levels = 1, bool from_tmem = false,
             u32 tmem_address_even = 0, u32 tmem_address_odd = 0);
  static void UnbindTextures();
  virtual void BindTextures();
  static void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, u32 dstStride,
                                        PEControl::PixelFormat srcFormat,
                                        const EFBRectangle& srcRect, bool isIntensity,
                                        bool scaleByHalf, u32 scaleToHeight = 0);

  virtual void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, void* palette,
                              TlutFormat format) = 0;

protected:
  TextureCacheBase();

  alignas(16) static u8* temp;
  static size_t temp_size;

  static TCacheEntry* bound_textures[8];

private:
  struct TexPoolEntry  // Minimal version of TCacheEntry just for TexPool
  {
    std::unique_ptr<AbstractTextureBase> texture;
    int frameCount = FRAMECOUNT_INVALID;
    TexPoolEntry(std::unique_ptr<AbstractTextureBase> tex) : texture(std::move(tex)) {}
  };

  typedef std::multimap<u64, TCacheEntry*> TexCache;
  typedef std::unordered_multimap<AbstractTextureBase::TextureConfig, TexPoolEntry,
                                  AbstractTextureBase::TextureConfig::Hasher>
      TexPool;
  static void ScaleTextureCacheEntryTo(TCacheEntry* entry, u32 new_width, u32 new_height);
  static TCacheEntry* DoPartialTextureUpdates(TexCache::iterator iter, u8* palette, u32 tlutfmt);
  static void DumpTexture(TCacheEntry* entry, std::string basename, unsigned int level);
  static void CheckTempSize(size_t required_size);

  static std::unique_ptr<AbstractTextureBase>
  AllocateTexture(const AbstractTextureBase::TextureConfig& config);
  static TexCache::iterator GetTexCacheIter(TCacheEntry* entry);

  virtual std::unique_ptr<AbstractTextureBase>
  CreateTexture(const AbstractTextureBase::TextureConfig& config) = 0;

  // Removes and unlinks texture from texture cache and returns it to the pool
  static TexCache::iterator InvalidateTexture(TexCache::iterator t_iter);

  static TexCache textures_by_address;
  static TexCache textures_by_hash;
  static TexPool texture_pool;

  // Backup configuration values
  static struct BackupConfig
  {
    int s_colorsamples;
    bool s_texfmt_overlay;
    bool s_texfmt_overlay_center;
    bool s_hires_textures;
    bool s_cache_hires_textures;
    bool s_copy_cache_enable;
    bool s_stereo_3d;
    bool s_efb_mono_depth;
  } backup_config;
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
