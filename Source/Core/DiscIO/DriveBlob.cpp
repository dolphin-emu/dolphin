// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DriveBlob.h"

#ifdef _WIN32
#include "Common/StringUtil.h"
#else
#include <stdio.h>  // fileno
#include <sys/ioctl.h>
#if defined __linux__
#include <linux/fs.h>  // BLKGETSIZE64
#elif defined __FreeBSD__
#include <sys/disk.h>  // DIOCGMEDIASIZE
#elif defined __APPLE__
#include <sys/disk.h>  // DKIOCGETBLOCKCOUNT / DKIOCGETBLOCKSIZE
#endif
#endif

namespace DiscIO
{
DriveReader::DriveReader(const std::string& drive)
{
  // 32 sectors is roughly the optimal amount a CD Drive can read in
  // a single IO cycle. Larger values yield no performance improvement
  // and just cause IO stalls from the read delay. Smaller values allow
  // the OS IO and seeking overhead to ourstrip the time actually spent
  // transferring bytes from the media.
  SetChunkSize(32);  // 32*2048 = 64KiB
  SetSectorSize(2048);
#ifdef _WIN32
  auto const path = UTF8ToTStr(std::string("\\\\.\\") + drive);
  m_disc_handle = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
  if (IsOK())
  {
    // Do a test read to make sure everything is OK, since it seems you can get
    // handles to empty drives.
    DWORD not_used;
    std::vector<u8> buffer(GetSectorSize());
    if (!ReadFile(m_disc_handle, buffer.data(), GetSectorSize(), &not_used, nullptr))
    {
      // OK, something is wrong.
      CloseHandle(m_disc_handle);
      m_disc_handle = INVALID_HANDLE_VALUE;
    }
  }
  if (IsOK())
  {
    // Initialize m_size by querying the volume capacity.
    STORAGE_READ_CAPACITY storage_size;
    storage_size.Version = sizeof(storage_size);
    DWORD bytes = 0;
    DeviceIoControl(m_disc_handle, IOCTL_STORAGE_READ_CAPACITY, nullptr, 0, &storage_size,
                    sizeof(storage_size), &bytes, nullptr);
    m_size = bytes ? storage_size.DiskLength.QuadPart : 0;

#ifdef _LOCKDRIVE  // Do we want to lock the drive?
    // Lock the compact disc in the CD-ROM drive to prevent accidental
    // removal while reading from it.
    m_lock_cdrom.PreventMediaRemoval = TRUE;
    DeviceIoControl(m_disc_handle, IOCTL_CDROM_MEDIA_REMOVAL, &m_lock_cdrom, sizeof(m_lock_cdrom),
                    nullptr, 0, &dwNotUsed, nullptr);
#endif
#else
  m_file.Open(drive, "rb");
  if (m_file)
  {
    int fd = fileno(m_file.GetHandle());
#if defined __linux__
    // NOTE: Doesn't matter if it fails, m_size was initialized to zero
    ioctl(fd, BLKGETSIZE64, &m_size);  // u64*
#elif defined __FreeBSD__
    off_t size = 0;
    ioctl(fd, DIOCGMEDIASIZE, &size);  // off_t*
    m_size = size;
#elif defined __APPLE__
    u64 count = 0;
    u32 block_size = 0;
    ioctl(fd, DKIOCGETBLOCKCOUNT, &count);      // u64*
    ioctl(fd, DKIOCGETBLOCKSIZE, &block_size);  // u32*
    m_size = count * block_size;
#endif
#endif
  }
  else { NOTICE_LOG(DISCIO, "Load from DVD backup failed or no disc in drive %s", drive.c_str()); }
}

DriveReader::~DriveReader()
{
#ifdef _WIN32
#ifdef _LOCKDRIVE  // Do we want to lock the drive?
  // Unlock the disc in the CD-ROM drive.
  m_lock_cdrom.PreventMediaRemoval = FALSE;
  DeviceIoControl(m_disc_handle, IOCTL_CDROM_MEDIA_REMOVAL, &m_lock_cdrom, sizeof(m_lock_cdrom),
                  nullptr, 0, &dwNotUsed, nullptr);
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

bool DriveReader::GetBlock(u64 block_num, u8* out_ptr)
{
  return DriveReader::ReadMultipleAlignedBlocks(block_num, 1, out_ptr);
}

bool DriveReader::ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8* out_ptr)
{
#ifdef _WIN32
  LARGE_INTEGER offset;
  offset.QuadPart = GetSectorSize() * block_num;
  DWORD bytes_read;
  if (!SetFilePointerEx(m_disc_handle, offset, nullptr, FILE_BEGIN) ||
      !ReadFile(m_disc_handle, out_ptr, static_cast<DWORD>(GetSectorSize() * num_blocks),
                &bytes_read, nullptr))
  {
    PanicAlertT("Disc Read Error");
    return false;
  }
  return bytes_read == GetSectorSize() * num_blocks;
#else
  m_file.Seek(GetSectorSize() * block_num, SEEK_SET);
  if (m_file.ReadBytes(out_ptr, num_blocks * GetSectorSize()))
    return true;
  m_file.Clear();
  return false;
#endif
}

}  // namespace
