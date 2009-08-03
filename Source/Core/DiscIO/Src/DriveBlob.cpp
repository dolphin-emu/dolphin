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

#include "stdafx.h"
#include "DriveBlob.h"

namespace DiscIO
{
	DriveReader::DriveReader(const char *drive)
	{
#ifdef _WIN32
		char path[MAX_PATH];
		strncpy(path, drive, 3);
		path[2] = 0;
		sprintf(path, "\\\\.\\%s", drive);
		SectorReader::SetSectorSize(2048);
		hDisc = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
		if (hDisc != INVALID_HANDLE_VALUE)
		{
			// Do a test read to make sure everything is OK, since it seems you can get
			// handles to empty drives.
			DWORD not_used;
			u8 *buffer = new u8[m_blocksize];
			if (!ReadFile(hDisc, buffer, m_blocksize, (LPDWORD)&not_used, NULL))
			{
				delete [] buffer;
				// OK, something is wrong.
				CloseHandle(hDisc);
				hDisc = INVALID_HANDLE_VALUE;
				return;
			}
			delete [] buffer;
			
		#ifdef _LOCKDRIVE // Do we want to lock the drive?
			// Lock the compact disc in the CD-ROM drive to prevent accidental
			// removal while reading from it.
			pmrLockCDROM.PreventMediaRemoval = TRUE;
			DeviceIoControl(hDisc, IOCTL_CDROM_MEDIA_REMOVAL,
                       &pmrLockCDROM, sizeof(pmrLockCDROM), NULL,
                       0, &dwNotUsed, NULL);
		#endif
#else
		file_ = fopen(drive, "rb");
		if (file_)
		{
#endif
		}
		else
		{
			PanicAlert("Load from DVD backup failed or no disc in drive %s", drive);
		}
	} // DriveReader::DriveReader

	DriveReader::~DriveReader()
	{
#ifdef _WIN32
	#ifdef _LOCKDRIVE // Do we want to lock the drive?
		// Unlock the disc in the CD-ROM drive.
		pmrLockCDROM.PreventMediaRemoval = FALSE;
		DeviceIoControl (hDisc, IOCTL_CDROM_MEDIA_REMOVAL,
			&pmrLockCDROM, sizeof(pmrLockCDROM), NULL,
			0, &dwNotUsed, NULL);
	#endif
		if (hDisc != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hDisc);
			hDisc = INVALID_HANDLE_VALUE;
		}
#else
		fclose(file_);
		file_ = 0;
#endif	
	}

	DriveReader *DriveReader::Create(const char *drive)
	{
		DriveReader *reader = new DriveReader(drive);
		if (!reader->IsOK())
		{
			delete reader;
			return 0;
		}
		return reader;
	}

	void DriveReader::GetBlock(u64 block_num, u8 *out_ptr)
	{
		u8 * lpSector = new u8[m_blocksize];
#ifdef _WIN32
		u32 NotUsed;
		u64 offset = m_blocksize * block_num;
		LONG off_low = (LONG)offset & 0xFFFFFFFF;
		LONG off_high = (LONG)(offset >> 32);
		SetFilePointer(hDisc, off_low, &off_high, FILE_BEGIN);
		if (!ReadFile(hDisc, lpSector, m_blocksize, (LPDWORD)&NotUsed, NULL))
			PanicAlert("Disc Read Error");
#else
		fseek(file_, m_blocksize*block_num, SEEK_SET);
		fread(lpSector, 1, m_blocksize, file_);
#endif
		memcpy(out_ptr, lpSector, m_blocksize);
		delete[] lpSector;
	}

	bool DriveReader::ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr)
	{
		u32 NotUsed;
#ifdef _WIN32
		u64 offset = m_blocksize * block_num;
		LONG off_low = (LONG)offset & 0xFFFFFFFF;
		LONG off_high = (LONG)(offset >> 32);
		SetFilePointer(hDisc, off_low, &off_high, FILE_BEGIN);
		if (!ReadFile(hDisc, out_ptr, (DWORD)(m_blocksize * num_blocks), (LPDWORD)&NotUsed, NULL))
		{
			PanicAlert("Disc Read Error");
			return false;
		}
#else
		fseek(file_, m_blocksize*block_num, SEEK_SET);
		if(fread(out_ptr, 1, m_blocksize * num_blocks, file_) != m_blocksize * num_blocks)
			return false;
#endif
		return true;
	}
}  // namespace
