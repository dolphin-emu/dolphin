// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DriveBlob.h"

#ifdef _WIN32
#include "Common/StringUtil.h"
#endif

namespace DiscIO
{

DriveReader::DriveReader(const std::string& drive)
{
#ifdef _WIN32
	SectorReader::SetSectorSize(2048);
	auto const path = UTF8ToTStr(std::string("\\\\.\\") + drive);
	m_disc_handle = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                           nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
	if (m_disc_handle != INVALID_HANDLE_VALUE)
	{
		// Do a test read to make sure everything is OK, since it seems you can get
		// handles to empty drives.
		DWORD not_used;
		u8 *buffer = new u8[m_blocksize];
		if (!ReadFile(m_disc_handle, buffer, m_blocksize, (LPDWORD)&not_used, nullptr))
		{
			delete [] buffer;
			// OK, something is wrong.
			CloseHandle(m_disc_handle);
			m_disc_handle = INVALID_HANDLE_VALUE;
			return;
		}
		delete [] buffer;

	#ifdef _LOCKDRIVE // Do we want to lock the drive?
		// Lock the compact disc in the CD-ROM drive to prevent accidental
		// removal while reading from it.
		m_lock_cdrom.PreventMediaRemoval = TRUE;
		DeviceIoControl(m_disc_handle, IOCTL_CDROM_MEDIA_REMOVAL,
		                &m_lock_cdrom, sizeof(m_lock_cdrom), nullptr,
		                0, &dwNotUsed, nullptr);
	#endif
#else
	SectorReader::SetSectorSize(2048);
	m_file.Open(drive, "rb");
	if (m_file)
	{
#endif
	}
	else
	{
		NOTICE_LOG(DISCIO, "Load from DVD backup failed or no disc in drive %s", drive.c_str());
	}
}

DriveReader::~DriveReader()
{
#ifdef _WIN32
#ifdef _LOCKDRIVE // Do we want to lock the drive?
	// Unlock the disc in the CD-ROM drive.
	m_lock_cdrom.PreventMediaRemoval = FALSE;
	DeviceIoControl (m_disc_handle, IOCTL_CDROM_MEDIA_REMOVAL,
		&m_lock_cdrom, sizeof(m_lock_cdrom), nullptr,
		0, &dwNotUsed, nullptr);
#endif
	if (m_disc_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_disc_handle);
		m_disc_handle = INVALID_HANDLE_VALUE;
	}
#else
	m_file.Close();
#endif
}

DriveReader* DriveReader::Create(const std::string& drive)
{
	DriveReader* reader = new DriveReader(drive);

	if (!reader->IsOK())
	{
		delete reader;
		return nullptr;
	}

	return reader;
}

void DriveReader::GetBlock(u64 block_num, u8* out_ptr)
{
	u8* const lpSector = new u8[m_blocksize];
#ifdef _WIN32
	u32 NotUsed;
	u64 offset = m_blocksize * block_num;
	LONG off_low = (LONG)offset & 0xFFFFFFFF;
	LONG off_high = (LONG)(offset >> 32);
	SetFilePointer(m_disc_handle, off_low, &off_high, FILE_BEGIN);
	if (!ReadFile(m_disc_handle, lpSector, m_blocksize, (LPDWORD)&NotUsed, nullptr))
		PanicAlertT("Disc Read Error");
#else
	m_file.Seek(m_blocksize * block_num, SEEK_SET);
	m_file.ReadBytes(lpSector, m_blocksize);
#endif
	memcpy(out_ptr, lpSector, m_blocksize);
	delete[] lpSector;
}

bool DriveReader::ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8* out_ptr)
{
#ifdef _WIN32
	u32 NotUsed;
	u64 offset = m_blocksize * block_num;
	LONG off_low = (LONG)offset & 0xFFFFFFFF;
	LONG off_high = (LONG)(offset >> 32);
	SetFilePointer(m_disc_handle, off_low, &off_high, FILE_BEGIN);
	if (!ReadFile(m_disc_handle, out_ptr, (DWORD)(m_blocksize * num_blocks), (LPDWORD)&NotUsed, nullptr))
	{
		PanicAlertT("Disc Read Error");
		return false;
	}
#else
	fseeko(m_file.GetHandle(), (m_blocksize * block_num), SEEK_SET);
	if (fread(out_ptr, 1, (m_blocksize * num_blocks), m_file.GetHandle()) != (m_blocksize * num_blocks))
		return false;
#endif
	return true;
}

}  // namespace
