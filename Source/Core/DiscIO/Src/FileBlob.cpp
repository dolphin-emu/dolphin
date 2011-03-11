// Copyright (C) 2003 Dolphin Project.

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

#include "FileBlob.h"

namespace DiscIO
{

PlainFileReader::PlainFileReader(std::FILE* file)
	: m_file(file)
{
	m_size = m_file.GetSize();
}

PlainFileReader* PlainFileReader::Create(const char* filename)
{
	File::IOFile f(filename, "rb");
	if (f)
		return new PlainFileReader(f.ReleaseHandle());
	else
		return NULL;
}

bool PlainFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
	m_file.Seek(offset, SEEK_SET);
	return m_file.ReadBytes(out_ptr, nbytes);
}

}  // namespace
