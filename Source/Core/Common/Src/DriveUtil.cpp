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

#include "Common.h"
#include "DriveUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

void GetAllRemovableDrives(std::vector<std::string> *drives) {
	drives->clear();
#ifdef _WIN32
	char drives_string[1024];
	int max_drive_pos = GetLogicalDriveStrings(1024, drives_string);
	char *p = drives_string;
	// GetLogicalDriveStrings returns the drives as a a series of null-terminated strings
	// laid out right after each other in RAM, with a double null at the end.
	while (*p)
	{
		if (GetDriveType(p) == DRIVE_CDROM)  // CD_ROM also applies to DVD. Noone has a plain CDROM without DVD anymore so we should be fine.
		{
			drives->push_back(std::string(p).substr(0, 2));
		}
		p += strlen(p) + 1;
	}
#else
	// TODO
	// stat("/media/cdrom") or whatever etc etc
#endif
}
