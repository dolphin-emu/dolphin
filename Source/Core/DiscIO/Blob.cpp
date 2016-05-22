// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>

#include "Common/CDUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#include "DiscIO/Blob.h"
#include "DiscIO/CISOBlob.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/DriveBlob.h"
#include "DiscIO/FileBlob.h"
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
	auto itr = std::find_if(m_cache.begin(), m_cache.end(), [&](const Cache& entry)
	{
		return entry.Contains(block_num);
	});
	if (itr == m_cache.end())
		return nullptr;

	itr->MarkUsed();
	return &*itr;
}

SectorReader::Cache* SectorReader::GetEmptyCacheLine()
{
	Cache* oldest = &m_cache[0];
	// Find the Least Recently Used cache line to replace.
	for (auto& cache_entry : m_cache)
	{
		if (cache_entry.IsLessRecentlyUsedThan(*oldest))
			oldest = &cache_entry;
		cache_entry.ShiftLRU();
	}
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
	u64 block  = 0;
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

		std::copy(cache->data.begin() + read_offset,
		          cache->data.begin() + read_offset + was_read,
		          out_ptr);

		offset  += was_read;
		out_ptr += was_read;
		remain  -= was_read;
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
	u64 block_num  = chunk_num * m_chunk_blocks;
	u32 cnt_blocks = m_chunk_blocks;

	// If we are reading the end of a disk, there may not be enough blocks to
	// read a whole chunk. We need to clamp down in that case.
	u64 end_block = GetDataSize() / m_block_size;
	if (end_block)
		cnt_blocks = static_cast<u32>(std::min<u64>(m_chunk_blocks, end_block - block_num));

	if (ReadMultipleAlignedBlocks(block_num, cnt_blocks, buffer))
	{
		if (cnt_blocks < m_chunk_blocks)
		{
			std::fill(buffer + cnt_blocks * m_block_size,
			          buffer + m_chunk_blocks * m_block_size,
			          0u);
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

std::unique_ptr<IBlobReader> CreateBlobReader(const std::string& filename)
{
	if (cdio_is_cdrom(filename))
		return DriveReader::Create(filename);

	if (!File::Exists(filename))
		return nullptr;

	if (IsWbfsBlob(filename))
		return WbfsFileReader::Create(filename);

	if (IsGCZBlob(filename))
		return CompressedBlobReader::Create(filename);

	if (IsCISOBlob(filename))
		return CISOFileReader::Create(filename);

	// Still here? Assume plain file - since we know it exists due to the File::Exists check above.
	return PlainFileReader::Create(filename);
}

}  // namespace
