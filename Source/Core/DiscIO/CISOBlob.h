// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{

bool IsCISOBlob(const std::string& filename);

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
	static std::unique_ptr<CISOFileReader> Create(const std::string& filename);

	BlobType GetBlobType() const override { return BlobType::CISO; }

	// The CISO format does not save the original file size.
	// This function returns an upper bound.
	u64 GetDataSize() const override;

	u64 GetRawSize() const override;
	bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

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
