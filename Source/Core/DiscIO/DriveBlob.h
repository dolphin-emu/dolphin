// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

namespace DiscIO
{

class DriveReader : public SectorReader
{
private:
	DriveReader(const std::string& drive);
	void GetBlock(u64 block_num, u8 *out_ptr) override;

#ifdef _WIN32
	HANDLE hDisc;
	PREVENT_MEDIA_REMOVAL pmrLockCDROM;
	bool IsOK() {return hDisc != INVALID_HANDLE_VALUE;}
#else
	File::IOFile file_;
	bool IsOK() {return file_ != nullptr;}
#endif
	s64 size;

public:
	static DriveReader* Create(const std::string& drive);
	~DriveReader();
	u64 GetDataSize() const override { return size; }
	u64 GetRawSize() const override { return size; }

	virtual bool ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr) override;
};

}  // namespace
