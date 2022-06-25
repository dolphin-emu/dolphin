// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureDecoder.h"

class AbstractFramebuffer;
class AbstractStagingTexture;
class AbstractTexture;
class PointerWrap;

namespace VideoCommon::TextureCache
{
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

struct CacheEntry
{
  static const int FRAMECOUNT_INVALID = 0;

  // common members
  std::unique_ptr<AbstractTexture> texture;
  std::unique_ptr<AbstractFramebuffer> framebuffer;
  u32 addr = 0;
  u32 size_in_bytes = 0;
  u64 base_hash = 0;
  u64 hash = 0;  // for paletted textures, hash = base_hash ^ palette_hash
  TextureAndTLUTFormat format;
  u32 memory_stride = 0;
  bool is_efb_copy = false;
  bool is_custom_tex = false;
  bool may_have_overlapping_textures = true;
  bool tmem_only = false;           // indicates that this texture only exists in the tmem cache
  bool has_arbitrary_mips = false;  // indicates that the mips in this texture are arbitrary
                                    // content, aren't just downscaled
  bool should_force_safe_hashing = false;  // for XFB
  bool is_xfb_copy = false;
  bool is_xfb_container = false;
  u64 id = 0;

  bool reference_changed = false;  // used by xfb to determine when a reference xfb changed

  // Texture dimensions from the GameCube's point of view
  u32 native_width = 0;
  u32 native_height = 0;
  u32 native_levels = 0;

  // used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
  int frameCount = FRAMECOUNT_INVALID;

  // Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when
  // removing the cache entry
  std::multimap<u64, CacheEntry*>::iterator textures_by_hash_iter;

  // This is used to keep track of both:
  //   * efb copies used by this partially updated texture
  //   * partially updated textures which refer to this efb copy
  std::unordered_set<CacheEntry*> references;

  // Pending EFB copy
  std::unique_ptr<AbstractStagingTexture> pending_efb_copy;
  u32 pending_efb_copy_width = 0;
  u32 pending_efb_copy_height = 0;
  bool pending_efb_copy_invalidated = false;

  std::string texture_info_name = "";

  explicit CacheEntry(std::unique_ptr<AbstractTexture> tex,
                      std::unique_ptr<AbstractFramebuffer> fb);

  ~CacheEntry();

  void SetGeneralParameters(u32 _addr, u32 _size, TextureAndTLUTFormat _format,
                            bool force_safe_hashing);

  void SetDimensions(unsigned int _native_width, unsigned int _native_height,
                     unsigned int _native_levels);

  void SetHashes(u64 _base_hash, u64 _hash);

  // This texture entry is used by the other entry as a sub-texture
  void CreateReference(CacheEntry* other_entry);

  void SetXfbCopy(u32 stride);
  void SetEfbCopy(u32 stride);
  void SetNotCopy();

  bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

  bool IsEfbCopy() const;
  bool IsCopy() const;
  u32 NumBlocksX() const;
  u32 NumBlocksY() const;
  u32 BytesPerRow() const;

  u64 CalculateHash() const;

  int HashSampleSize() const;
  u32 GetWidth() const;
  u32 GetHeight() const;
  u32 GetNumLevels() const;
  u32 GetNumLayers() const;
  AbstractTextureFormat GetFormat() const;
  void DoState(PointerWrap& p);
};
}  // namespace VideoCommon::TextureCache
