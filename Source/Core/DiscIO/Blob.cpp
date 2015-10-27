// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
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

// Provides caching and split-operation-to-block-operations facilities.
// Used for compressed blob reading and direct drive reading.

void SectorReader::SetSectorSize(int blocksize)
{
	for (int i = 0; i < CACHE_SIZE; i++)
	{
		m_cache[i] = new u8[blocksize];
		m_cache_tags[i] = (u64)(s64) - 1;
	}
	m_blocksize = blocksize;
}

SectorReader::~SectorReader()
{
	for (u8*& block : m_cache)
	{
		delete [] block;
	}
}

const u8 *SectorReader::GetBlockData(u64 block_num)
{
	// TODO : Expand usage of the cache to more than one block :P
	if (m_cache_tags[0] == block_num)
	{
		return m_cache[0];
	}
	else
	{
		GetBlock(block_num, m_cache[0]);
		m_cache_tags[0] = block_num;
		return m_cache[0];
	}
}

bool SectorReader::Read(u64 offset, u64 size, u8* out_ptr)
{
	u64 startingBlock = offset / m_blocksize;
	u64 remain = size;

	int positionInBlock = (int)(offset % m_blocksize);
	u64 block = startingBlock;

	while (remain > 0)
	{
		// Check if we are ready to do a large block read. > instead of >= so we don't bother if remain is only one block.
		if (positionInBlock == 0 && remain > (u64)m_blocksize)
		{
			u64 num_blocks = remain / m_blocksize;
			ReadMultipleAlignedBlocks(block, num_blocks, out_ptr);
			block += num_blocks;
			out_ptr += num_blocks * m_blocksize;
			remain -= num_blocks * m_blocksize;
			continue;
		}

		const u8* data = GetBlockData(block);
		if (!data)
			return false;

		u32 toCopy = m_blocksize - positionInBlock;
		if (toCopy >= remain)
		{
			// Yay, we are done!
			memcpy(out_ptr, data + positionInBlock, (size_t)remain);
			return true;
		}
		else
		{
			memcpy(out_ptr, data + positionInBlock, toCopy);
			out_ptr += toCopy;
			remain -= toCopy;
			positionInBlock = 0;
			block++;
		}
	}

	return true;
}

bool SectorReader::ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr)
{
	for (u64 i = 0; i < num_blocks; i++)
	{
		const u8 *data = GetBlockData(block_num + i);
		if (!data)
			return false;
		memcpy(out_ptr + i * m_blocksize, data, m_blocksize);
	}

	return true;
}

IBlobReader* CreateBlobReader(const std::string& filename)
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
