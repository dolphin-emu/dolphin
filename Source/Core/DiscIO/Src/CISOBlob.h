// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CISO_BLOB_H
#define _CISO_BLOB_H

#include "Blob.h"
#include "FileUtil.h"

namespace DiscIO
{

bool IsCISOBlob(const char* filename);

static const u32 CISO_HEADER_SIZE = 0x8000;
static const u32 CISO_MAP_SIZE = CISO_HEADER_SIZE - sizeof(u32) - sizeof(char) * 4;

struct CISOHeader
{
	// "CISO"
	char magic[4];
	
	// little endian
	u32 block_size;
	
	// 0=unused, 1=used, others=invalid
	u8 map[CISO_MAP_SIZE];
};

class CISOFileReader : public IBlobReader
{
public:
	static CISOFileReader* Create(const char* filename);

	u64 GetDataSize() const;
	u64 GetRawSize() const;
	bool Read(u64 offset, u64 nbytes, u8* out_ptr);

private:
	CISOFileReader(std::FILE* file);
	
	typedef u16 MapType;
	static const MapType UNUSED_BLOCK_ID = -1;
	
	File::IOFile m_file;
	u64 m_size;
	u32 m_block_size;
	MapType m_ciso_map[CISO_MAP_SIZE];
};

}  // namespace

#endif  // _FILE_BLOB_H
