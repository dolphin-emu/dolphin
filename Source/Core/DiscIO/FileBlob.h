// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstdio>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{

class PlainFileReader : public IBlobReader
{
	PlainFileReader(std::FILE* file);

	File::IOFile m_file;
	s64 m_size;

public:
	static PlainFileReader* Create(const char* filename);

	u64 GetDataSize() const { return m_size; }
	u64 GetRawSize() const { return m_size; }
	bool Read(u64 offset, u64 nbytes, u8* out_ptr);
};

}  // namespace
