// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <unordered_set>

#include "Common/CommonTypes.h"

#include "VideoCommon/TextureDecoder.h"

class AbstractTexture;
enum class AbstractTextureFormat : u32;

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

struct TCacheEntry
{
  static const int FRAMECOUNT_INVALID = 0;

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
  bool should_force_safe_hashing = false;  // for XFB
  bool is_xfb_copy = false;
  float y_scale = 1.0f;
  float gamma = 1.0f;
  u64 id;

  bool reference_changed = false;  // used by xfb to determine when a reference xfb changed

  unsigned int native_width, native_height;  // Texture dimensions from the GameCube's point of view
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
  u32 GetWidth() const;
  u32 GetHeight() const;
  u32 GetNumLevels() const;
  u32 GetNumLayers() const;
  AbstractTextureFormat GetFormat() const;
};
