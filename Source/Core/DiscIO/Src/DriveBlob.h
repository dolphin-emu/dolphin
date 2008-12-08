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
#endif

namespace DiscIO
{

#ifdef _WIN32
class DriveReader : public SectorReader
{
	HANDLE hDisc;

private:
	DriveReader(const char *drive) {
		/*
		char path[MAX_PATH];
		strncpy(path, drive, 3);
		path[2] = 0;
		sprintf(path, "\\\\.\\%s", drive);
		hDisc = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		SetSectorSize(2048);
		*/
	}

public:
	static DriveReader *Create(const char *drive) {
		return NULL;// new DriveReader(drive);		
	}

};

#endif



}  // namespace

#endif  // _DRIVE_BLOB_H
