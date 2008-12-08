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

#include "Common.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

void GetAllRemovableDrives(std::vector<std::string> *drives) {
	drives->clear();
#ifdef _WIN32
	HANDLE hDisk;
	DISK_GEOMETRY diskGeometry;

	for (int i = 'A'; i < 'Z'; i++)
	{
		char path[MAX_PATH];
		sprintf(path, "\\\\.\\%c:", i);
		hDisk = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hDisk != INVALID_HANDLE_VALUE)
		{
			DWORD dwBytes;
			DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &diskGeometry, sizeof(DISK_GEOMETRY), &dwBytes, NULL);
			// Only proceed if disk is a removable media
			if (diskGeometry.MediaType == RemovableMedia)
			{
				if (diskGeometry.BytesPerSector == 2048) {
					// Probably CD/DVD drive.
					// "Remove" the "\\.\" part of the path and return it.
					drives->push_back(path + 4);	
				}
			}
		}
		CloseHandle(hDisk);
	}
#else
	// TODO
	// stat("/media/cdrom") or whatever etc etc
#endif
}

