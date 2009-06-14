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

#include "SDCardUtil.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* believe me, you *don't* want to change these constants !! */
#define BYTES_PER_SECTOR	512
#define RESERVED_SECTORS	32
#define BACKUP_BOOT_SECTOR	6
#define NUM_FATS			2

#define BYTE_(p,i)      (((u8*)(p))[(i)])

#define POKEB(p,v)	BYTE_(p,0) = (u8)(v)
#define POKES(p,v)	( BYTE_(p,0) = (u8)(v), BYTE_(p,1) = (u8)((v) >> 8) )
#define POKEW(p,v)	( BYTE_(p,0) = (u8)(v), BYTE_(p,1) = (u8)((v) >> 8), BYTE_(p,2) = (u8)((v) >> 16), BYTE_(p,3) = (u8)((v) >> 24) )

static u8 s_boot_sector		[ BYTES_PER_SECTOR ];	/* boot sector */
static u8 s_fsinfo_sector	[ BYTES_PER_SECTOR ];	/* FS Info sector */
static u8 s_fat_head		[ BYTES_PER_SECTOR ];	/* first FAT sector */
static u8 s_zero_sector		[ BYTES_PER_SECTOR ];	/* empty sector */

/* this is the date and time when creating the disk */
static int get_serial_id()
{
	u16			lo, hi;
	time_t		now = time(NULL);
	struct tm	tm  = gmtime( &now )[0];

	lo = (u16)(tm.tm_mday + ((tm.tm_mon+1) << 8) + (tm.tm_sec << 8));
	hi = (u16)(tm.tm_min + (tm.tm_hour << 8) + (tm.tm_year + 1900));

	return lo + (hi << 16);
}

static int get_sectors_per_cluster(u64 disk_size)
{
	u64 disk_MB = disk_size/(1024*1024);

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

static int get_sectors_per_fat(u64 disk_size, int sectors_per_cluster)
{
	u64 divider;

	/* weird computation from MS - see fatgen103.doc for details */
	disk_size -= RESERVED_SECTORS * BYTES_PER_SECTOR;	/* don't count 32 reserved sectors */
	disk_size /= BYTES_PER_SECTOR;	/* disk size in sectors */
	divider = ((256 * sectors_per_cluster) + NUM_FATS) / 2;

	return (int)( (disk_size + (divider-1)) / divider );
}

static void boot_sector_init(u8* boot, u8* info, u64 disk_size, const char* label)
{
	int sectors_per_cluster	= get_sectors_per_cluster(disk_size);
	int sectors_per_fat		= get_sectors_per_fat(disk_size, sectors_per_cluster);
	int sectors_per_disk	= (int)(disk_size / BYTES_PER_SECTOR);
	int serial_id			= get_serial_id();
	int free_count;

	if (label == NULL)
		label = "DOLPHINSD";

	POKEB(boot, 0xeb);
	POKEB(boot+1, 0x5a);
	POKEB(boot+2, 0x90);
	strcpy( (char*)boot + 3, "MSWIN4.1" );
	POKES( boot + 0x0b, BYTES_PER_SECTOR );		/* sector size */
	POKEB( boot + 0xd, sectors_per_cluster );	/* sectors per cluster */
	POKES( boot + 0xe, RESERVED_SECTORS );		/* reserved sectors before first FAT */
	POKEB( boot + 0x10, NUM_FATS );				/* number of FATs */
	POKES( boot + 0x11, 0 );					/* max root directory entries for FAT12/FAT16, 0 for FAT32 */
	POKES( boot + 0x13, 0 );					/* total sectors, 0 to use 32-bit value at offset 0x20 */
	POKEB( boot + 0x15, 0xF8 );					/* media descriptor, 0xF8 == hard disk */
	POKES( boot + 0x16, 0 );					/* Sectors per FAT for FAT12/16, 0 for FAT32 */
	POKES( boot + 0x18, 9 );					/* Sectors per track (whatever) */
	POKES( boot + 0x1a, 2 );					/* Number of heads (whatever) */
	POKEW( boot + 0x1c, 0 );					/* Hidden sectors */
	POKEW( boot + 0x20, sectors_per_disk );		/* Total sectors */

	/* extension */
	POKEW( boot + 0x24, sectors_per_fat );		/* Sectors per FAT */
	POKES( boot + 0x28, 0 );					/* FAT flags */
	POKES( boot + 0x2a, 0 );					/* version */
	POKEW( boot + 0x2c, 2 );					/* cluster number of root directory start */
	POKES( boot + 0x30, 1 );					/* sector number of FS information sector */
	POKES( boot + 0x32, BACKUP_BOOT_SECTOR );	/* sector number of a copy of this boot sector */
	POKEB( boot + 0x40, 0x80 );					/* physical drive number */
	POKEB( boot + 0x42, 0x29 );					/* extended boot signature ?? */
	POKEW( boot + 0x43, serial_id );			/* serial ID */
	strncpy( (char*)boot + 0x47, label, 11 );	/* Volume Label */
	memcpy( boot + 0x52, "FAT32   ", 8 );		/* FAT system type, padded with 0x20 */

	POKEB( boot + BYTES_PER_SECTOR-2, 0x55 );	/* boot sector signature */
	POKEB( boot + BYTES_PER_SECTOR-1, 0xAA );

	/* FSInfo sector */
	free_count = sectors_per_disk - 32 - 2*sectors_per_fat;

	POKEW( info + 0,   0x41615252 );
	POKEW( info + 484, 0x61417272 );
	POKEW( info + 488, free_count );	/* number of free clusters */
	POKEW( info + 492, 3 );				/* next free clusters, 0-1 reserved, 2 is used for the root dir */
	POKEW( info + 508, 0xAA550000 );
}

static void fat_init(u8* fat)
{
	POKEW( fat,     0x0ffffff8 );	/* reserve cluster 1, media id in low byte */
	POKEW( fat + 4, 0x0fffffff );	/* reserve cluster 2 */
	POKEW( fat + 8, 0x0fffffff );	/* end of clust chain for root dir */
}


static int write_sector(FILE* file, u8* sector)
{
	return fwrite(sector, 1, 512, file) != 512;
}

static int write_empty(FILE* file, u64 count)
{
	static u8 empty[64*1024];

	count *= 512;
	while (count > 0)
	{
		u64 len = sizeof(empty);
		if (len > count)
			len = count;

		if ( fwrite(empty, 1, (size_t)len, file) != (size_t)len )
			return 1;

		count -= len;
	}
	return 0;
}

bool SDCardCreate(u32 disk_size /*in MB*/, char* filename)
{
	int sectors_per_fat;
	int sectors_per_disk;
	FILE* f;

	// Convert MB to bytes
	disk_size *= 1024*1024;

	if (disk_size < 0x800000 || disk_size > 0x800000000ULL)
		ERROR_LOG(COMMON, "Trying to create SD Card image of size %iMB is out of range (8MB-32GB)", disk_size/(1024*1024));

	sectors_per_disk = disk_size / 512;
	sectors_per_fat  = get_sectors_per_fat(disk_size, get_sectors_per_cluster(disk_size));

	boot_sector_init(s_boot_sector, s_fsinfo_sector, disk_size, NULL );
	fat_init(s_fat_head);

	f = fopen(filename, "wb");
	if (!f)
		ERROR_LOG(COMMON, "could not create file '%s', aborting...\n", filename);

	/* here's the layout:
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

	if (write_sector(f, s_boot_sector))		goto FailWrite;
	if (write_sector(f, s_fsinfo_sector))	goto FailWrite;
	if (BACKUP_BOOT_SECTOR > 0)
	{
		if (write_empty(f, BACKUP_BOOT_SECTOR - 2)) goto FailWrite;
		if (write_sector(f, s_boot_sector))		goto FailWrite;
		if (write_sector(f, s_fsinfo_sector))	goto FailWrite;
		if (write_empty(f, RESERVED_SECTORS - 2 - BACKUP_BOOT_SECTOR)) goto FailWrite;
	}
	else
		if (write_empty(f, RESERVED_SECTORS - 2)) goto FailWrite;

	if (write_sector(f, s_fat_head))		goto FailWrite;
	if (write_empty(f, sectors_per_fat-1))	goto FailWrite;

	if (write_sector(f, s_fat_head))		goto FailWrite;
	if (write_empty(f, sectors_per_fat-1))	goto FailWrite;

	if (write_empty(f, sectors_per_disk - RESERVED_SECTORS - 2*sectors_per_fat)) goto FailWrite;

	fclose(f);
	return true;

FailWrite:
	ERROR_LOG(COMMON, "could not write to '%s', aborting...\n", filename);
	unlink(filename);
	fclose(f);
	return false;
}
