// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// BLOB

// Blobs in Dolphin are read only Binary Large OBjects. For example, a typical DVD image.
// Often, you may want to store these things in a highly compressed format, but still
// allow random access. Or you may store them on an odd device, like raw on a DVD.

// Always read your BLOBs using an interface returned by CreateBlobReader(). It will
// detect whether the file is a compressed blob, or just a big hunk of data, or a drive, and
// automatically do the right thing.

#include <string>
#include "Common/CommonTypes.h"

namespace DiscIO
{

class IBlobReader
{
public:
	virtual ~IBlobReader() {}

	virtual u64 GetRawSize() const  = 0;
	virtual u64 GetDataSize() const = 0;
	// NOT thread-safe - can't call this from multiple threads.
	virtual bool Read(u64 offset, u64 size, u8* out_ptr) = 0;

protected:
	IBlobReader() {}
};


// Provides caching and split-operation-to-block-operations facilities.
// Used for compressed blob reading and direct drive reading.
// Currently only uses a single entry cache.
// Multi-block reads are not cached.
class SectorReader : public IBlobReader
{
public:
	virtual ~SectorReader();

	// A pointer returned by GetBlockData is invalidated as soon as GetBlockData, Read, or ReadMultipleAlignedBlocks is called again.
	const u8 *GetBlockData(u64 block_num);
	virtual bool Read(u64 offset, u64 size, u8 *out_ptr) override;
	friend class DriveReader;

protected:
	void SetSectorSize(int blocksize);
	virtual void GetBlock(u64 block_num, u8 *out) = 0;
	// This one is uncached. The default implementation is to simply call GetBlockData multiple times and memcpy.
	virtual bool ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr);

private:
	enum { CACHE_SIZE = 32 };
	int m_blocksize;
	u8* m_cache[CACHE_SIZE];
	u64 m_cache_tags[CACHE_SIZE];
	int m_cache_age[CACHE_SIZE];
};

// Factory function - examines the path to choose the right type of IBlobReader, and returns one.
IBlobReader* CreateBlobReader(const std::string& filename);

typedef bool (*CompressCB)(const std::string& text, float percent, void* arg);

bool CompressFileToBlob(const std::string& infile, const std::string& outfile, u32 sub_type = 0, int sector_size = 16384,
		CompressCB callback = nullptr, void *arg = nullptr);
bool DecompressBlobToFile(const std::string& infile, const std::string& outfile,
		CompressCB callback = nullptr, void *arg = nullptr);

}  // namespace
