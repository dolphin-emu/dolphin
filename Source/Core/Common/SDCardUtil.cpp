// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

/* mksdcard.c
**
** Copyright 2007, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of Google Inc. nor the names of its contributors may
**       be used to endorse or promote products derived from this software
**       without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY Google Inc. ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
** EVENT SHALL Google Inc. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
** PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
** OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
** OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
** ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// A simple and portable piece of code used to generate a blank FAT32 image file.
// Modified for Dolphin.

#include "Common/SDCardUtil.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

#ifndef _WIN32
#include <unistd.h>  // for unlink()
#endif

namespace Common
{
// Believe me, you *don't* want to change these constants !!
enum : u32
{
  BYTES_PER_SECTOR = 512,
  RESERVED_SECTORS = 32,
  BACKUP_BOOT_SECTOR = 6,
  NUM_FATS = 2
};

static u8 s_boot_sector[BYTES_PER_SECTOR];   /* Boot sector */
static u8 s_fsinfo_sector[BYTES_PER_SECTOR]; /* FS Info sector */
static u8 s_fat_head[BYTES_PER_SECTOR];      /* First FAT sector */

template <typename T>
static void WriteData(u8* out, T data)
{
  std::memcpy(out, &data, sizeof(data));
}

/* This is the date and time when creating the disk */
static unsigned int get_serial_id()
{
  const time_t now = std::time(nullptr);
  const struct tm tm = std::gmtime(&now)[0];

  const u16 lo = static_cast<u16>(tm.tm_mday + ((tm.tm_mon + 1) << 8) + (tm.tm_sec << 8));
  const u16 hi = static_cast<u16>(tm.tm_min + (tm.tm_hour << 8) + (tm.tm_year + 1900));

  return lo + (hi << 16);
}

static unsigned int get_sectors_per_cluster(u64 disk_size)
{
  u64 disk_MB = disk_size / (1024 * 1024);

  if (disk_MB < 260)
    return 1;

  if (disk_MB < 8192)
    return 4;

  if (disk_MB < 16384)
    return 8;

  if (disk_MB < 32768)
    return 16;

  return 32;
}

static unsigned int get_sectors_per_fat(u64 disk_size, u32 sectors_per_cluster)
{
  /* Weird computation from MS - see fatgen103.doc for details */
  disk_size -= RESERVED_SECTORS * BYTES_PER_SECTOR; /* Don't count 32 reserved sectors */
  disk_size /= BYTES_PER_SECTOR;                    /* Disk size in sectors */
  const u64 divider = ((256 * sectors_per_cluster) + NUM_FATS) / 2;

  return static_cast<u32>((disk_size + (divider - 1)) / divider);
}

static void boot_sector_init(u8* boot, u8* info, u64 disk_size, const char* label)
{
  const u32 sectors_per_cluster = get_sectors_per_cluster(disk_size);
  const u32 sectors_per_fat = get_sectors_per_fat(disk_size, sectors_per_cluster);
  const u32 sectors_per_disk = static_cast<u32>(disk_size / BYTES_PER_SECTOR);
  const u32 serial_id = get_serial_id();

  if (label == nullptr)
    label = "DOLPHINSD";

  WriteData<u8>(boot, 0xeb);
  WriteData<u8>(boot + 1, 0x5a);
  WriteData<u8>(boot + 2, 0x90);
  strcpy((char*)boot + 3, "MSWIN4.1");
  WriteData<u16>(boot + 0x0b, BYTES_PER_SECTOR);   // Sector size
  WriteData<u8>(boot + 0xd, sectors_per_cluster);  // Sectors per cluster
  WriteData<u16>(boot + 0xe, RESERVED_SECTORS);    // Reserved sectors before first FAT
  WriteData<u8>(boot + 0x10, NUM_FATS);            // Number of FATs
  WriteData<u16>(boot + 0x11, 0);    // Max root directory entries for FAT12/FAT16, 0 for FAT32
  WriteData<u16>(boot + 0x13, 0);    // Total sectors, 0 to use 32-bit value at offset 0x20
  WriteData<u8>(boot + 0x15, 0xF8);  // Media descriptor, 0xF8 == hard disk
  WriteData<u16>(boot + 0x16, 0);    // Sectors per FAT for FAT12/16, 0 for FAT32
  WriteData<u16>(boot + 0x18, 9);    // Sectors per track (whatever)
  WriteData<u16>(boot + 0x1a, 2);    // Number of heads (whatever)
  WriteData<u32>(boot + 0x1c, 0);    // Hidden sectors
  WriteData<u32>(boot + 0x20, sectors_per_disk);  // Total sectors

  // Extension
  WriteData<u32>(boot + 0x24, sectors_per_fat);     // Sectors per FAT
  WriteData<u16>(boot + 0x28, 0);                   // FAT flags
  WriteData<u16>(boot + 0x2a, 0);                   // Version
  WriteData<u32>(boot + 0x2c, 2);                   // Cluster number of root directory start
  WriteData<u16>(boot + 0x30, 1);                   // Sector number of FS information sector
  WriteData<u16>(boot + 0x32, BACKUP_BOOT_SECTOR);  // Sector number of a copy of this boot sector
  WriteData<u8>(boot + 0x40, 0x80);                 // Physical drive number
  WriteData<u8>(boot + 0x42, 0x29);                 // Extended boot signature ??
  WriteData<u32>(boot + 0x43, serial_id);           // Serial ID
  strncpy((char*)boot + 0x47, label, 11);           // Volume Label
  memcpy(boot + 0x52, "FAT32   ", 8);               // FAT system type, padded with 0x20

  WriteData<u8>(boot + BYTES_PER_SECTOR - 2, 0x55);  // Boot sector signature
  WriteData<u8>(boot + BYTES_PER_SECTOR - 1, 0xAA);

  /* FSInfo sector */
  const u32 free_count = sectors_per_disk - 32 - 2 * sectors_per_fat;

  WriteData<u32>(info + 0, 0x41615252);
  WriteData<u32>(info + 484, 0x61417272);
  WriteData<u32>(info + 488, free_count);  // Number of free clusters
  // Next free clusters, 0-1 reserved, 2 is used for the root dir
  WriteData<u32>(info + 492, 3);
  WriteData<u32>(info + 508, 0xAA550000);
}

static void fat_init(u8* fat)
{
  WriteData<u32>(fat, 0x0ffffff8);      // Reserve cluster 1, media id in low byte
  WriteData<u32>(fat + 4, 0x0fffffff);  // Reserve cluster 2
  WriteData<u32>(fat + 8, 0x0fffffff);  // End of cluster chain for root dir
}

static bool write_sector(File::IOFile& file, const u8* sector)
{
  return file.WriteBytes(sector, BYTES_PER_SECTOR);
}

static bool write_empty(File::IOFile& file, std::size_t count)
{
  static constexpr u8 empty[64 * 1024] = {};

  count *= BYTES_PER_SECTOR;
  while (count > 0)
  {
    const std::size_t len = std::min(sizeof(empty), count);

    if (!file.WriteBytes(empty, len))
      return false;

    count -= len;
  }
  return true;
}

bool SDCardCreate(u64 disk_size /*in MB*/, const std::string& filename)
{
  // Convert MB to bytes
  disk_size *= 1024 * 1024;

  if (disk_size < 0x800000 || disk_size > 0x800000000ULL)
  {
    ERROR_LOG_FMT(COMMON, "Trying to create SD Card image of size {}MB is out of range (8MB-32GB)",
                  disk_size / (1024 * 1024));
    return false;
  }

  // Pretty unlikely to overflow.
  const u32 sectors_per_disk = static_cast<u32>(disk_size / BYTES_PER_SECTOR);
  const u32 sectors_per_fat = get_sectors_per_fat(disk_size, get_sectors_per_cluster(disk_size));

  boot_sector_init(s_boot_sector, s_fsinfo_sector, disk_size, nullptr);
  fat_init(s_fat_head);

  File::IOFile file(filename, "wb");
  if (!file)
  {
    ERROR_LOG_FMT(COMMON, "Could not create file '{}', aborting...", filename);
    return false;
  }

  /* Here's the layout:
   *
   *  boot_sector
   *  fsinfo_sector
   *  empty
   *  backup boot sector
   *  backup fsinfo sector
   *  RESERVED_SECTORS - 4 empty sectors (if backup sectors), or RESERVED_SECTORS - 2 (if no backup)
   *  first fat
   *  second fat
   *  zero sectors
   */

  if (!write_sector(file, s_boot_sector))
    goto FailWrite;

  if (!write_sector(file, s_fsinfo_sector))
    goto FailWrite;

  if constexpr (BACKUP_BOOT_SECTOR > 0)
  {
    if (!write_empty(file, BACKUP_BOOT_SECTOR - 2))
      goto FailWrite;

    if (!write_sector(file, s_boot_sector))
      goto FailWrite;

    if (!write_sector(file, s_fsinfo_sector))
      goto FailWrite;

    if (!write_empty(file, RESERVED_SECTORS - 2 - BACKUP_BOOT_SECTOR))
      goto FailWrite;
  }
  else
  {
    if (!write_empty(file, RESERVED_SECTORS - 2))
      goto FailWrite;
  }

  if (!write_sector(file, s_fat_head))
    goto FailWrite;

  if (!write_empty(file, sectors_per_fat - 1))
    goto FailWrite;

  if (!write_sector(file, s_fat_head))
    goto FailWrite;

  if (!write_empty(file, sectors_per_fat - 1))
    goto FailWrite;

  if (!write_empty(file, sectors_per_disk - RESERVED_SECTORS - 2 * sectors_per_fat))
    goto FailWrite;

  return true;

FailWrite:
  ERROR_LOG_FMT(COMMON, "Could not write to '{}', aborting...", filename);
  if (unlink(filename.c_str()) < 0)
    ERROR_LOG_FMT(COMMON, "unlink({}) failed: {}", filename, LastStrerrorString());
  return false;
}
}  // namespace Common
