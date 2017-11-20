// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/Hash.h"
#include "Common/MemoryUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/TextureCacheEntry.h"
#include "VideoCommon/VideoConfig.h"

TCacheEntry::TCacheEntry(std::unique_ptr<AbstractTexture> tex) : texture(std::move(tex))
{
}

TCacheEntry::~TCacheEntry()
{
  for (auto& reference : references)
    reference->references.erase(this);
}

bool TCacheEntry::OverlapsMemoryRange(u32 range_address, u32 range_size) const
{
  if (addr + size_in_bytes <= range_address)
    return false;

  if (addr >= range_address + range_size)
    return false;

  return true;
}

u32 TCacheEntry::BytesPerRow() const
{
  const u32 blockW = TexDecoder_GetBlockWidthInTexels(format.texfmt);

  // Round up source height to multiple of block size
  const u32 actualWidth = Common::AlignUp(native_width, blockW);

  const u32 numBlocksX = actualWidth / blockW;

  // RGBA takes two cache lines per block; all others take one
  const u32 bytes_per_block = format == TextureFormat::RGBA8 ? 64 : 32;

  return numBlocksX * bytes_per_block;
}

u32 TCacheEntry::NumBlocksY() const
{
  u32 blockH = TexDecoder_GetBlockHeightInTexels(format.texfmt);
  // Round up source height to multiple of block size
  u32 actualHeight = Common::AlignUp(static_cast<unsigned int>(native_height * y_scale), blockH);

  return actualHeight / blockH;
}

void TCacheEntry::SetXfbCopy(u32 stride)
{
  is_efb_copy = false;
  is_xfb_copy = true;
  memory_stride = stride;

  _assert_msg_(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void TCacheEntry::SetEfbCopy(u32 stride)
{
  is_efb_copy = true;
  is_xfb_copy = false;
  memory_stride = stride;

  _assert_msg_(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void TCacheEntry::SetNotCopy()
{
  is_xfb_copy = false;
  is_efb_copy = false;
}

int TCacheEntry::HashSampleSize() const
{
  if (should_force_safe_hashing)
  {
    return 0;
  }

  return g_ActiveConfig.iSafeTextureCache_ColorSamples;
}

u64 TCacheEntry::CalculateHash() const
{
  u8* ptr = Memory::GetPointer(addr);
  if (memory_stride == BytesPerRow())
  {
    return GetHash64(ptr, size_in_bytes, HashSampleSize());
  }
  else
  {
    u32 blocks = NumBlocksY();
    u64 temp_hash = size_in_bytes;

    u32 samples_per_row = 0;
    if (HashSampleSize() != 0)
    {
      // Hash at least 4 samples per row to avoid hashing in a bad pattern, like just on the left
      // side of the efb copy
      samples_per_row = std::max(HashSampleSize() / blocks, 4u);
    }

    for (u32 i = 0; i < blocks; i++)
    {
      // Multiply by a prime number to mix the hash up a bit. This prevents identical blocks from
      // canceling each other out
      temp_hash = (temp_hash * 397) ^ GetHash64(ptr, BytesPerRow(), samples_per_row);
      ptr += memory_stride;
    }
    return temp_hash;
  }
}

u32 TCacheEntry::GetWidth() const
{
  return texture->GetConfig().width;
}

u32 TCacheEntry::GetHeight() const
{
  return texture->GetConfig().height;
}

u32 TCacheEntry::GetNumLevels() const
{
  return texture->GetConfig().levels;
}

u32 TCacheEntry::GetNumLayers() const
{
  return texture->GetConfig().layers;
}

AbstractTextureFormat TCacheEntry::GetFormat() const
{
  return texture->GetConfig().format;
}
