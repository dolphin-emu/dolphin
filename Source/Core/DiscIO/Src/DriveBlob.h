// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DRIVE_BLOB_H
#define _DRIVE_BLOB_H

#include "Blob.h"
#include "FileUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

namespace DiscIO
{

class DriveReader : public SectorReader
{
private:
	DriveReader(const char *drive);
	void GetBlock(u64 block_num, u8 *out_ptr);

#ifdef _WIN32
	HANDLE hDisc;
	PREVENT_MEDIA_REMOVAL pmrLockCDROM;
	bool IsOK() {return hDisc != INVALID_HANDLE_VALUE;}
#else
	File::IOFile file_;
	bool IsOK() {return file_ != 0;}
#endif
	s64 size;
	u64 *block_pointers;

public:
	static DriveReader *Create(const char *drive);
	~DriveReader();
	u64 GetDataSize() const { return size; }
	u64 GetRawSize() const { return size; }

	virtual bool ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr);
};

}  // namespace

#endif  // _DRIVE_BLOB_H
