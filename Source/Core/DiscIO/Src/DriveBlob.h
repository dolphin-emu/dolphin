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

#ifndef _DRIVE_BLOB_H
#define _DRIVE_BLOB_H

#include "Blob.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

namespace DiscIO
{

class DriveReader : public SectorReader
{
private:
	DriveReader(const char *drive);
	void GetBlock(u64 block_num, u8 *out_ptr);

#ifdef _WIN32
	HANDLE hDisc;
	PREVENT_MEDIA_REMOVAL pmrLockCDROM;
	bool IsOK() {return hDisc != INVALID_HANDLE_VALUE;}
#else
	FILE* file_;
	bool IsOK() {return file_ != 0;}
#endif
	s64 size;
	u64 *block_pointers;

public:
	static DriveReader *Create(const char *drive);
	~DriveReader();
	u64 GetDataSize() const { return size; }
	u64 GetRawSize() const { return size; }

	virtual bool ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr);
};

}  // namespace

#endif  // _DRIVE_BLOB_H
