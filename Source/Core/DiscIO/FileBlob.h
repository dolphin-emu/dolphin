// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _FILE_BLOB_H
#define _FILE_BLOB_H

#include "Blob.h"
#include "FileUtil.h"

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

#endif  // _FILE_BLOB_H
