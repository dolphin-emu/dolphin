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

		hDisc = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
		if (hDisc == INVALID_HANDLE_VALUE)
		{		
			PanicAlert("Load from DVD backup failed");
		}
		else
		{
			SectorReader::SetSectorSize(2048);
		#ifdef _LOCKDRIVE
			// Lock the compact disc in the CD-ROM drive to prevent accidental
			// removal while reading from it.
			pmrLockCDROM.PreventMediaRemoval = TRUE;
			DeviceIoControl (hDisc, IOCTL_CDROM_MEDIA_REMOVAL,
                       &pmrLockCDROM, sizeof(pmrLockCDROM), NULL,
                       0, &dwNotUsed, NULL);
		#endif
		}
#else
		file_ = fopen(drive, "rb");
		if (!file_)
				PanicAlert("Load from DVD backup failed");
		else
			SetSectorSize(2048);
#endif
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
		CloseHandle(hDisc);
#else
		fclose(file_);
#endif	
	} // DriveReader::~DriveReader


	DriveReader * DriveReader::Create(const char *drive)
	{
		return new DriveReader(drive);
	}

	bool DriveReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
	{
		u64 startingBlock = offset / m_blocksize;
		u64 remain = nbytes;
		int positionInBlock = (int)(offset % m_blocksize);
		u64 block = startingBlock;

		while (remain > 0)
		{
			const u8* data = GetBlockData(block);
			if (!data)
				return false;
			
			u32 toCopy = m_blocksize - positionInBlock;
			if (toCopy >= remain)
			{
				// yay, we are done!
				memcpy(out_ptr, data + positionInBlock, (size_t)remain);
				return true;
			}
			else
			{
				memcpy(out_ptr, data + positionInBlock, toCopy);
				out_ptr += toCopy;
				remain -= toCopy;
				positionInBlock = 0;
				block++;
			}
		}
		return true;
	} // DriveReader::Read

	void DriveReader::GetBlock(u64 block_num, u8 *out_ptr)
	{
		u32 NotUsed;
		u8 * lpSector = new u8[m_blocksize];
#ifdef _WIN32
		u64 offset = m_blocksize * block_num;
		LONG off_low = (LONG)offset & 0xFFFFFFFF;
		LONG off_high = (LONG)(offset >> 32);
		SetFilePointer(hDisc, off_low, &off_high, FILE_BEGIN);
		if (!ReadFile(hDisc, lpSector, m_blocksize, (LPDWORD)&NotUsed, NULL))
			PanicAlert("DRE");
#else
		fseek(file_, m_blocksize*block_num, SEEK_SET);
		fread(lpSector, 1, m_blocksize, file_);
#endif
		
		memcpy(out_ptr, lpSector, m_blocksize);
		delete lpSector;
	}

	const u8 *DriveReader::GetBlockData(u64 block_num)
	{
		if (SectorReader::cache_tags[0] == block_num)
		{
			return SectorReader::cache[0];
		}
		else 
		{
			GetBlock(block_num, cache[0]);
			SectorReader::cache_tags[0] = block_num;
			return SectorReader::cache[0];
		}
	}

}  // namespace
