// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "Common/CDUtils.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"

#include "DiscIO/Blob.h"
#include "DiscIO/CISOBlob.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/DriveBlob.h"
#include "DiscIO/FileBlob.h"
#include "DiscIO/TGCBlob.h"
#include "DiscIO/WbfsBlob.h"

namespace DiscIO
{
void SectorReader::SetSectorSize(int blocksize)
{
  m_block_size = std::max(blocksize, 0);
  for (auto& cache_entry : m_cache)
  {
    cache_entry.Reset();
    cache_entry.data.resize(m_chunk_blocks * m_block_size);
  }
}

void SectorReader::SetChunkSize(int block_cnt)
{
  m_chunk_blocks = std::max(block_cnt, 1);
  // Clear cache and resize the data arrays
  SetSectorSize(m_block_size);
}

SectorReader::~SectorReader()
{
}

const SectorReader::Cache* SectorReader::FindCacheLine(u64 block_num)
{
  auto itr = std::find_if(m_cache.begin(), m_cache.end(),
                          [&](const Cache& entry) { return entry.Contains(block_num); });
  if (itr == m_cache.end())
    return nullptr;

  itr->MarkUsed();
  return &*itr;
}

SectorReader::Cache* SectorReader::GetEmptyCacheLine()
{
  Cache* oldest = &m_cache[0];
  // Find the Least Recently Used cache line to replace.
  std::for_each(m_cache.begin() + 1, m_cache.end(), [&](Cache& line) {
    if (line.IsLessRecentlyUsedThan(*oldest))
    {
      oldest->ShiftLRU();
      oldest = &line;
      return;
    }
    line.ShiftLRU();
  });
  oldest->Reset();
  return oldest;
}

const SectorReader::Cache* SectorReader::GetCacheLine(u64 block_num)
{
  if (auto entry = FindCacheLine(block_num))
    return entry;

  // Cache miss. Fault in the missing entry.
  Cache* cache = GetEmptyCacheLine();
  // We only read aligned chunks, this avoids duplicate overlapping entries.
  u64 chunk_idx = block_num / m_chunk_blocks;
  u32 blocks_read = ReadChunk(cache->data.data(), chunk_idx);
  if (!blocks_read)
    return nullptr;
  cache->Fill(chunk_idx * m_chunk_blocks, blocks_read);

  // Secondary check for out-of-bounds read.
  // If we got less than m_chunk_blocks, we may still have missed.
  // We do this after the cache fill since the cache line itself is
  // fine, the problem is being asked to read past the end of the disk.
  return cache->Contains(block_num) ? cache : nullptr;
}

bool SectorReader::Read(u64 offset, u64 size, u8* out_ptr)
{
  u64 remain = size;
  u64 block = 0;
  u32 position_in_block = static_cast<u32>(offset % m_block_size);

  while (remain > 0)
  {
    block = offset / m_block_size;

    const Cache* cache = GetCacheLine(block);
    if (!cache)
      return false;

    // Cache entries are aligned chunks, we may not want to read from the start
    u32 read_offset = static_cast<u32>(block - cache->block_idx) * m_block_size + position_in_block;
    u32 can_read = m_block_size * cache->num_blocks - read_offset;
    u32 was_read = static_cast<u32>(std::min<u64>(can_read, remain));

    std::copy(cache->data.begin() + read_offset, cache->data.begin() + read_offset + was_read,
              out_ptr);

    offset += was_read;
    out_ptr += was_read;
    remain -= was_read;
    position_in_block = 0;
  }
  return true;
}

// Crap default implementation if not overridden.
bool SectorReader::ReadMultipleAlignedBlocks(u64 block_num, u64 cnt_blocks, u8* out_ptr)
{
  for (u64 i = 0; i < cnt_blocks; ++i)
  {
    if (!GetBlock(block_num + i, out_ptr))
      return false;
    out_ptr += m_block_size;
  }
  return true;
}

u32 SectorReader::ReadChunk(u8* buffer, u64 chunk_num)
{
  u64 block_num = chunk_num * m_chunk_blocks;
  u32 cnt_blocks = m_chunk_blocks;

  // If we are reading the end of a disk, there may not be enough blocks to
  // read a whole chunk. We need to clamp down in that case.
  u64 end_block = (GetDataSize() + m_block_size - 1) / m_block_size;
  if (end_block)
    cnt_blocks = static_cast<u32>(std::min<u64>(m_chunk_blocks, end_block - block_num));

  if (ReadMultipleAlignedBlocks(block_num, cnt_blocks, buffer))
  {
    if (cnt_blocks < m_chunk_blocks)
    {
      std::fill(buffer + cnt_blocks * m_block_size, buffer + m_chunk_blocks * m_block_size, 0u);
    }
    return cnt_blocks;
  }

  // end_block may be zero on real disks if we fail to get the media size.
  // We have to fallback to probing the disk instead.
  if (!end_block)
  {
    for (u32 i = 0; i < cnt_blocks; ++i)
    {
      if (!GetBlock(block_num + i, buffer))
      {
        std::fill(buffer, buffer + (cnt_blocks - i) * m_block_size, 0u);
        return i;
      }
      buffer += m_block_size;
    }
    return cnt_blocks;
  }
  return 0;
}

std::unique_ptr<BlobReader> CreateBlobReader(const std::string& filename)
{
  if (cdio_is_cdrom(filename))
    return DriveReader::Create(filename);

  File::IOFile file(filename, "rb");
  u32 magic;
  if (!file.ReadArray(&magic, 1))
    return nullptr;

  // Conveniently, every supported file format (except for plain disc images and
  // extracted discs) starts with a 4-byte magic number that identifies the format,
  // so we just need a simple switch statement to create the right blob type. If the
  // magic number doesn't match any known magic number and the directory structure
  // doesn't match the directory blob format, we assume it's a plain disc image. If
  // that assumption is wrong, the volume code that runs later will notice the error
  // because the blob won't provide the right data when reading the GC/Wii disc header.

  switch (magic)
  {
  case CISO_MAGIC:
    return CISOFileReader::Create(std::move(file));
  case GCZ_MAGIC:
    return CompressedBlobReader::Create(std::move(file), filename);
  case TGC_MAGIC:
    return TGCFileReader::Create(std::move(file));
  case WBFS_MAGIC:
    return WbfsFileReader::Create(std::move(file), filename);
  default:
    if (auto directory_blob = DirectoryBlobReader::Create(filename))
      return std::move(directory_blob);

    return PlainFileReader::Create(std::move(file));
  }
}

}  // namespace
