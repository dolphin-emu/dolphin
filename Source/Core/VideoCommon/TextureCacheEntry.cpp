// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureCacheEntry.h"

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/Hash.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon::TextureCache
{
CacheEntry::CacheEntry(std::unique_ptr<AbstractTexture> tex,
                       std::unique_ptr<AbstractFramebuffer> fb)
    : texture(std::move(tex)), framebuffer(std::move(fb))
{
}

CacheEntry::~CacheEntry()
{
  for (auto& reference : references)
    reference->references.erase(this);
}

void CacheEntry::SetGeneralParameters(u32 _addr, u32 _size, TextureAndTLUTFormat _format,
                                      bool force_safe_hashing)
{
  addr = _addr;
  size_in_bytes = _size;
  format = _format;
  should_force_safe_hashing = force_safe_hashing;
}

void CacheEntry::SetDimensions(unsigned int _native_width, unsigned int _native_height,
                               unsigned int _native_levels)
{
  native_width = _native_width;
  native_height = _native_height;
  native_levels = _native_levels;
  memory_stride = _native_width;
}

void CacheEntry::SetHashes(u64 _base_hash, u64 _hash)
{
  base_hash = _base_hash;
  hash = _hash;
}

void CacheEntry::CreateReference(CacheEntry* other_entry)
{
  // References are two-way, so they can easily be destroyed later
  this->references.emplace(other_entry);
  other_entry->references.emplace(this);
}

void CacheEntry::SetXfbCopy(u32 stride)
{
  is_efb_copy = false;
  is_xfb_copy = true;
  is_xfb_container = false;
  memory_stride = stride;

  ASSERT_MSG(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void CacheEntry::SetEfbCopy(u32 stride)
{
  is_efb_copy = true;
  is_xfb_copy = false;
  is_xfb_container = false;
  memory_stride = stride;

  ASSERT_MSG(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void CacheEntry::SetNotCopy()
{
  is_efb_copy = false;
  is_xfb_copy = false;
  is_xfb_container = false;
}

bool CacheEntry::OverlapsMemoryRange(u32 range_address, u32 range_size) const
{
  if (addr + size_in_bytes <= range_address)
    return false;

  if (addr >= range_address + range_size)
    return false;

  return true;
}

bool CacheEntry::IsEfbCopy() const
{
  return is_efb_copy;
}

bool CacheEntry::IsCopy() const
{
  return is_xfb_copy || is_efb_copy;
}

u32 CacheEntry::NumBlocksX() const
{
  const u32 blockW = TexDecoder_GetBlockWidthInTexels(format.texfmt);

  // Round up source height to multiple of block size
  const u32 actualWidth = Common::AlignUp(native_width, blockW);

  return actualWidth / blockW;
}

u32 CacheEntry::NumBlocksY() const
{
  u32 blockH = TexDecoder_GetBlockHeightInTexels(format.texfmt);
  // Round up source height to multiple of block size
  u32 actualHeight = Common::AlignUp(native_height, blockH);

  return actualHeight / blockH;
}

u32 CacheEntry::BytesPerRow() const
{
  // RGBA takes two cache lines per block; all others take one
  const u32 bytes_per_block = format == TextureFormat::RGBA8 ? 64 : 32;

  return NumBlocksX() * bytes_per_block;
}

u64 CacheEntry::CalculateHash() const
{
  const u32 bytes_per_row = BytesPerRow();
  const u32 hash_sample_size = HashSampleSize();

  // FIXME: textures from tmem won't get the correct hash.
  u8* ptr = Memory::GetPointer(addr);
  if (memory_stride == bytes_per_row)
  {
    return Common::GetHash64(ptr, size_in_bytes, hash_sample_size);
  }
  else
  {
    const u32 num_blocks_y = NumBlocksY();
    u64 temp_hash = size_in_bytes;

    u32 samples_per_row = 0;
    if (hash_sample_size != 0)
    {
      // Hash at least 4 samples per row to avoid hashing in a bad pattern, like just on the left
      // side of the efb copy
      samples_per_row = std::max(hash_sample_size / num_blocks_y, 4u);
    }

    for (u32 i = 0; i < num_blocks_y; i++)
    {
      // Multiply by a prime number to mix the hash up a bit. This prevents identical blocks from
      // canceling each other out
      temp_hash = (temp_hash * 397) ^ Common::GetHash64(ptr, bytes_per_row, samples_per_row);
      ptr += memory_stride;
    }
    return temp_hash;
  }
}

int CacheEntry::HashSampleSize() const
{
  if (should_force_safe_hashing)
  {
    return 0;
  }

  return g_ActiveConfig.iSafeTextureCache_ColorSamples;
}

u32 CacheEntry::GetWidth() const
{
  return texture->GetConfig().width;
}

u32 CacheEntry::GetHeight() const
{
  return texture->GetConfig().height;
}

u32 CacheEntry::GetNumLevels() const
{
  return texture->GetConfig().levels;
}

u32 CacheEntry::GetNumLayers() const
{
  return texture->GetConfig().layers;
}

AbstractTextureFormat CacheEntry::GetFormat() const
{
  return texture->GetConfig().format;
}

void CacheEntry::DoState(PointerWrap& p)
{
  p.Do(addr);
  p.Do(size_in_bytes);
  p.Do(base_hash);
  p.Do(hash);
  p.Do(format);
  p.Do(memory_stride);
  p.Do(is_efb_copy);
  p.Do(is_custom_tex);
  p.Do(may_have_overlapping_textures);
  p.Do(tmem_only);
  p.Do(has_arbitrary_mips);
  p.Do(should_force_safe_hashing);
  p.Do(is_xfb_copy);
  p.Do(is_xfb_container);
  p.Do(id);
  p.Do(reference_changed);
  p.Do(native_width);
  p.Do(native_height);
  p.Do(native_levels);
  p.Do(frameCount);
}

}  // namespace VideoCommon::TextureCache
