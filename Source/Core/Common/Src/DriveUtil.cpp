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

#include <string>
#include "DriveUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#elif defined(__GNUC__)		// TODO: replace these with real linux / os x functinos
#define GetLogicalDriveStrings(x, y) (int)memcpy(y,"/dev/cdrom\0/dev/cdrom1\0/dev/cdrom2\0/dev/cdrom3\0/dev/cdrom4\0/dev/cdrom5\0/dev/cdrom6\0/dev/cdrom7\0/dev/cdrom8\0/dev/cdrom9\0\0\0", 121);
#else
#define GetLogicalDriveStrings(x, y) (int)memcpy(y,"/dev/disk2\0/dev/disk3\0/dev/disk4\0/dev/disk5\0/dev/disk6\0/dev/disk7\0/dev/disk8\0/dev/disk9\0/dev/disk10\0/dev/disk11\0\0\0",114);
#endif

void GetAllRemovableDrives(std::vector<std::string> *drives) {
	drives->clear();
	char drives_string[1024];
	int max_drive_pos = GetLogicalDriveStrings(1024, drives_string);

	char *p = drives_string;
	// GetLogicalDriveStrings returns the drives as a a series of null-terminated strings
	// laid out right after each other in RAM, with a double null at the end.
	while (*p)
	{
		if (File::IsDisk(p))
		{
			drives->push_back(std::string(p));
		}
		p += strlen(p) + 1;
	}
}
