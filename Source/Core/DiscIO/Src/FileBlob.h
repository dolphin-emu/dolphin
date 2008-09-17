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

#ifndef _FILE_BLOB_H
#define _FILE_BLOB_H

#include "Blob.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace DiscIO
{

class PlainFileReader : public IBlobReader
{
#ifdef _WIN32
	HANDLE hFile;
	s64 size;
	PlainFileReader(HANDLE hFile_);
#endif

public:
	static PlainFileReader* Create(const char* filename);
	~PlainFileReader();
	u64 GetDataSize() const { return size; }
	u64 GetRawSize() const { return size; }
	bool Read(u64 offset, u64 size, u8* out_ptr);
};

}  // namespace

#endif  // _FILE_BLOB_H