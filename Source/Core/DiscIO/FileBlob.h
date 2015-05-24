// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{

class PlainFileReader : public IBlobReader
{
public:
	static PlainFileReader* Create(const std::string& filename);

	u64 GetDataSize() const override { return m_size; }
	u64 GetRawSize() const override { return m_size; }
	bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
	PlainFileReader(std::FILE* file);

	File::IOFile m_file;
	s64 m_size;
};

}  // namespace
