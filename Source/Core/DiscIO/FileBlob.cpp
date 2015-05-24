// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include "DiscIO/FileBlob.h"

namespace DiscIO
{

PlainFileReader::PlainFileReader(std::FILE* file)
	: m_file(file)
{
	m_size = m_file.GetSize();
}

PlainFileReader* PlainFileReader::Create(const std::string& filename)
{
	File::IOFile f(filename, "rb");
	if (f)
		return new PlainFileReader(f.ReleaseHandle());
	else
		return nullptr;
}

bool PlainFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
	if (m_file.Seek(offset, SEEK_SET) && m_file.ReadBytes(out_ptr, nbytes))
	{
		return true;
	}
	else
	{
		m_file.Clear();
		return false;
	}
}

}  // namespace
