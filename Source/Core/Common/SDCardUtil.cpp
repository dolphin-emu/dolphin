// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"
#include "Common/SDCardUtil.h"

#ifndef _WIN32
#include <unistd.h>  // for unlink()
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4310)
#endif

/* Believe me, you *don't* want to change these constants !! */
#define BYTES_PER_SECTOR 512
#define RESERVED_SECTORS 32
#define BACKUP_BOOT_SECTOR 6
#define NUM_FATS 2

#define BYTE_(p, i) (((u8*)(p))[(i)])

#define POKEB(p, v) BYTE_(p, 0) = (u8)(v)
#define POKES(p, v) (BYTE_(p, 0) = (u8)(v), BYTE_(p, 1) = (u8)((v) >> 8))
#define POKEW(p, v)                                                                                \
  (BYTE_(p, 0) = (u8)(v), BYTE_(p, 1) = (u8)((v) >> 8), BYTE_(p, 2) = (u8)((v) >> 16),             \
   BYTE_(p, 3) = (u8)((v) >> 24))

static u8 s_boot_sector[BYTES_PER_SECTOR];   /* Boot sector */
static u8 s_fsinfo_sector[BYTES_PER_SECTOR]; /* FS Info sector */
static u8 s_fat_head[BYTES_PER_SECTOR];      /* First FAT sector */

/* This is the date and time when creating the disk */
static unsigned int get_serial_id()
{
  u16 lo, hi;
  time_t now = time(nullptr);
  struct tm tm = gmtime(&now)[0];

  lo = (u16)(tm.tm_mday + ((tm.tm_mon + 1) << 8) + (tm.tm_sec << 8));
  hi = (u16)(tm.tm_min + (tm.tm_hour << 8) + (tm.tm_year + 1900));

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
  u64 divider;

  /* Weird computation from MS - see fatgen103.doc for details */
  disk_size -= RESERVED_SECTORS * BYTES_PER_SECTOR; /* Don't count 32 reserved sectors */
  disk_size /= BYTES_PER_SECTOR;                    /* Disk size in sectors */
  divider = ((256 * sectors_per_cluster) + NUM_FATS) / 2;

  return (u32)((disk_size + (divider - 1)) / divider);
}

static void boot_sector_init(u8* boot, u8* info, u64 disk_size, const char* label)
{
  u32 sectors_per_cluster = get_sectors_per_cluster(disk_size);
  u32 sectors_per_fat = get_sectors_per_fat(disk_size, sectors_per_cluster);
  u32 sectors_per_disk = (u32)(disk_size / BYTES_PER_SECTOR);
  u32 serial_id = get_serial_id();
  u32 free_count;

  if (label == nullptr)
    label = "DOLPHINSD";

  POKEB(boot, 0xeb);
  POKEB(boot + 1, 0x5a);
  POKEB(boot + 2, 0x90);
  strcpy((char*)boot + 3, "MSWIN4.1");
  POKES(boot + 0x0b, BYTES_PER_SECTOR);   /* Sector size */
  POKEB(boot + 0xd, sectors_per_cluster); /* Sectors per cluster */
  POKES(boot + 0xe, RESERVED_SECTORS);    /* Reserved sectors before first FAT */
  POKEB(boot + 0x10, NUM_FATS);           /* Number of FATs */
  POKES(boot + 0x11, 0);    /* Max root directory entries for FAT12/FAT16, 0 for FAT32 */
  POKES(boot + 0x13, 0);    /* Total sectors, 0 to use 32-bit value at offset 0x20 */
  POKEB(boot + 0x15, 0xF8); /* Media descriptor, 0xF8 == hard disk */
  POKES(boot + 0x16, 0);    /* Sectors per FAT for FAT12/16, 0 for FAT32 */
  POKES(boot + 0x18, 9);    /* Sectors per track (whatever) */
  POKES(boot + 0x1a, 2);    /* Number of heads (whatever) */
  POKEW(boot + 0x1c, 0);    /* Hidden sectors */
  POKEW(boot + 0x20, sectors_per_disk); /* Total sectors */

  /* Extension */
  POKEW(boot + 0x24, sectors_per_fat);    /* Sectors per FAT */
  POKES(boot + 0x28, 0);                  /* FAT flags */
  POKES(boot + 0x2a, 0);                  /* Version */
  POKEW(boot + 0x2c, 2);                  /* Cluster number of root directory start */
  POKES(boot + 0x30, 1);                  /* Sector number of FS information sector */
  POKES(boot + 0x32, BACKUP_BOOT_SECTOR); /* Sector number of a copy of this boot sector */
  POKEB(boot + 0x40, 0x80);               /* Physical drive number */
  POKEB(boot + 0x42, 0x29);               /* Extended boot signature ?? */
  POKEW(boot + 0x43, serial_id);          /* Serial ID */
  strncpy((char*)boot + 0x47, label, 11); /* Volume Label */
  memcpy(boot + 0x52, "FAT32   ", 8);     /* FAT system type, padded with 0x20 */

  POKEB(boot + BYTES_PER_SECTOR - 2, 0x55); /* Boot sector signature */
  POKEB(boot + BYTES_PER_SECTOR - 1, 0xAA);

  /* FSInfo sector */
  free_count = sectors_per_disk - 32 - 2 * sectors_per_fat;

  POKEW(info + 0, 0x41615252);
  POKEW(info + 484, 0x61417272);
  POKEW(info + 488, free_count); /* Number of free clusters */
  POKEW(info + 492, 3);          /* Next free clusters, 0-1 reserved, 2 is used for the root dir */
  POKEW(info + 508, 0xAA550000);
}

static void fat_init(u8* fat)
{
  POKEW(fat, 0x0ffffff8);     /* Reserve cluster 1, media id in low byte */
  POKEW(fat + 4, 0x0fffffff); /* Reserve cluster 2 */
  POKEW(fat + 8, 0x0fffffff); /* End of cluster chain for root dir */
}

static unsigned int write_sector(FILE* file, u8* sector)
{
  return fwrite(sector, 1, 512, file) != 512;
}

static unsigned int write_empty(FILE* file, u64 count)
{
  static u8 empty[64 * 1024];

  count *= 512;
  while (count > 0)
  {
    u64 len = sizeof(empty);
    if (len > count)
      len = count;

    if (fwrite(empty, 1, (size_t)len, file) != (size_t)len)
      return 1;

    count -= len;
  }
  return 0;
}

bool SDCardCreate(u64 disk_size /*in MB*/, const std::string& filename)
{
  u32 sectors_per_fat;
  u32 sectors_per_disk;

  // Convert MB to bytes
  disk_size *= 1024 * 1024;

  if (disk_size < 0x800000 || disk_size > 0x800000000ULL)
  {
    ERROR_LOG(COMMON,
              "Trying to create SD Card image of size %" PRIu64 "MB is out of range (8MB-32GB)",
              disk_size / (1024 * 1024));
    return false;
  }

  // Pretty unlikely to overflow.
  sectors_per_disk = (u32)(disk_size / 512);
  sectors_per_fat = get_sectors_per_fat(disk_size, get_sectors_per_cluster(disk_size));

  boot_sector_init(s_boot_sector, s_fsinfo_sector, disk_size, nullptr);
  fat_init(s_fat_head);

  File::IOFile file(filename, "wb");
  FILE* const f = file.GetHandle();
  if (!f)
  {
    ERROR_LOG(COMMON, "Could not create file '%s', aborting...", filename.c_str());
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

  if (write_sector(f, s_boot_sector))
    goto FailWrite;

  if (write_sector(f, s_fsinfo_sector))
    goto FailWrite;

  if (BACKUP_BOOT_SECTOR > 0)
  {
    if (write_empty(f, BACKUP_BOOT_SECTOR - 2))
      goto FailWrite;

    if (write_sector(f, s_boot_sector))
      goto FailWrite;

    if (write_sector(f, s_fsinfo_sector))
      goto FailWrite;

    if (write_empty(f, RESERVED_SECTORS - 2 - BACKUP_BOOT_SECTOR))
      goto FailWrite;
  }
  else
  {
    if (write_empty(f, RESERVED_SECTORS - 2))
      goto FailWrite;
  }

  if (write_sector(f, s_fat_head))
    goto FailWrite;

  if (write_empty(f, sectors_per_fat - 1))
    goto FailWrite;

  if (write_sector(f, s_fat_head))
    goto FailWrite;

  if (write_empty(f, sectors_per_fat - 1))
    goto FailWrite;

  if (write_empty(f, sectors_per_disk - RESERVED_SECTORS - 2 * sectors_per_fat))
    goto FailWrite;

  return true;

FailWrite:
  ERROR_LOG(COMMON, "Could not write to '%s', aborting...", filename.c_str());
  if (unlink(filename.c_str()) < 0)
    ERROR_LOG(COMMON, "unlink(%s) failed: %s", filename.c_str(), LastStrerrorString().c_str());
  return false;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
