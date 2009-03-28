// Copyright (C) 2003-2009 Dolphin Project.

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

// ------------------------------------------------------
// Handles giant memory mapped files
// Through some trickery, allows lock on byte boundaries
// instead of allocation granularity boundaries
// for ease of use
// ------------------------------------------------------

#ifndef _MAPPED_FILE_H_
#define _MAPPED_FILE_H_

#include <map>

namespace Common
{
class IMappedFile
{
public:
	virtual ~IMappedFile() {}

	virtual bool Open(const char* _szFilename) = 0;
	virtual bool IsOpen(void) = 0;
	virtual void Close(void)  = 0;
	virtual u64 GetSize(void) = 0;
	virtual u8* Lock(u64 _offset, u64 _size) = 0;
	virtual void Unlock(u8* ptr) = 0;

	static IMappedFile* CreateMappedFileDEPRECATED();
};

}  // namespace

#endif // _MAPPED_FILE_H_
