// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// WARNING Code not big-endian safe.

// To create new compressed BLOBs, use CompressFileToBlob.

// File format
// * Header
// * [Block Pointers interleaved with block hashes (hash of decompressed data)]
// * [Data]

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{

bool IsCompressedBlob(const std::string& filename);

const u32 kBlobCookie = 0xB10BC001;

// A blob file structure:
// BlobHeader
// u64 offsetsToBlocks[n], top bit specifies whether the block is compressed, or not.
// compressed data

// Blocks that won't compress to less than 97% of the original size are stored as-is.
struct CompressedBlobHeader // 32 bytes
{
	u32 magic_cookie; //0xB10BB10B
	u32 sub_type; // GC image, whatever
	u64 compressed_data_size;
	u64 data_size;
	u32 block_size;
	u32 num_blocks;
};

class CompressedBlobReader : public SectorReader
{
public:
	static CompressedBlobReader* Create(const std::string& filename);
	~CompressedBlobReader();
	const CompressedBlobHeader &GetHeader() const { return m_header; }
	u64 GetDataSize() const override { return m_header.data_size; }
	u64 GetRawSize() const override { return m_file_size; }
	u64 GetBlockCompressedSize(u64 block_num) const;
	void GetBlock(u64 block_num, u8* out_ptr) override;
private:
	CompressedBlobReader(const std::string& filename);

	CompressedBlobHeader m_header;
	u64* m_block_pointers;
	u32* m_hashes;
	int m_data_offset;
	File::IOFile m_file;
	u64 m_file_size;
	u8* m_zlib_buffer;
	int m_zlib_buffer_size;
	std::string m_file_name;
};

}  // namespace
