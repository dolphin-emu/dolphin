// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


// WARNING Code not big-endian safe.

// To create new compressed BLOBs, use CompressFileToBlob.

// File format
// * Header
// * [Block Pointers interleaved with block hashes (hash of decompressed data)]
// * [Data]

#ifndef COMPRESSED_BLOB_H_
#define COMPRESSED_BLOB_H_

#include <stdio.h>
#include "Blob.h"

namespace DiscIO
{

bool IsCompressedBlob(const char* filename);

const u32 kBlobCookie = 0xB10BC001;

// A blob file structure:
// BlobHeader
// u64 offsetsToBlocks[n], top bit specifies whether the block is compressed, or not.
// compressed data

// Blocks that won't compress to less than 97% of the original size are stored as-is.
struct CompressedBlobHeader // 32 bytes
{
	u32 magic_cookie; //0xB10BB10B
	u32 sub_type; // gc image, whatever
	u64 compressed_data_size;
	u64 data_size;
	u32 block_size;
	u32 num_blocks;
};

class CompressedBlobReader : public SectorReader
{
private:
	CompressedBlobHeader header;
	u64 *block_pointers;
	u32 *hashes;
	int data_offset;
	FILE *file;
	u64 file_size;
	u8 *zlib_buffer;
	int zlib_buffer_size;

	CompressedBlobReader(const char *filename);

public:
	static CompressedBlobReader* Create(const char *filename);
	~CompressedBlobReader();
	const CompressedBlobHeader &GetHeader() const { return header; }
	u64 GetDataSize() const { return header.data_size; }
	u64 GetRawSize() const { return file_size; }
	u64 GetBlockCompressedSize(u64 block_num) const;
	void GetBlock(u64 block_num, u8 *out_ptr);
};

}  // namespace

#endif  // COMPRESSED_BLOB_H_
