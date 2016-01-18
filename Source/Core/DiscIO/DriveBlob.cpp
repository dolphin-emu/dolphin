// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
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
		std::vector<u8> buffer(m_blocksize);
		if (!ReadFile(m_disc_handle, buffer.data(), m_blocksize, &not_used, nullptr))
		{
			// OK, something is wrong.
			CloseHandle(m_disc_handle);
			m_disc_handle = INVALID_HANDLE_VALUE;
			return;
		}

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

std::unique_ptr<DriveReader> DriveReader::Create(const std::string& drive)
{
	auto reader = std::unique_ptr<DriveReader>(new DriveReader(drive));

	if (!reader->IsOK())
		reader.reset();

	return reader;
}

void DriveReader::GetBlock(u64 block_num, u8* out_ptr)
{
	std::vector<u8> sector(m_blocksize);

#ifdef _WIN32
	u64 offset = m_blocksize * block_num;
	LONG off_low = (LONG)offset & 0xFFFFFFFF;
	LONG off_high = (LONG)(offset >> 32);
	DWORD not_used;
	SetFilePointer(m_disc_handle, off_low, &off_high, FILE_BEGIN);
	if (!ReadFile(m_disc_handle, sector.data(), m_blocksize, &not_used, nullptr))
		PanicAlertT("Disc Read Error");
#else
	m_file.Seek(m_blocksize * block_num, SEEK_SET);
	m_file.ReadBytes(sector.data(), m_blocksize);
#endif

	std::copy(sector.begin(), sector.end(), out_ptr);
}

bool DriveReader::ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8* out_ptr)
{
#ifdef _WIN32
	u64 offset = m_blocksize * block_num;
	LONG off_low = (LONG)offset & 0xFFFFFFFF;
	LONG off_high = (LONG)(offset >> 32);
	DWORD not_used;
	SetFilePointer(m_disc_handle, off_low, &off_high, FILE_BEGIN);
	if (!ReadFile(m_disc_handle, out_ptr, (DWORD)(m_blocksize * num_blocks), &not_used, nullptr))
	{
		PanicAlertT("Disc Read Error");
		return false;
	}
#else
	m_file.Seek(m_blocksize * block_num, SEEK_SET);
	if (!m_file.ReadBytes(out_ptr, m_blocksize * num_blocks))
		return false;
#endif
	return true;
}

}  // namespace
