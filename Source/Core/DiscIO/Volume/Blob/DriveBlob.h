// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Volume/Blob/Blob.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

namespace DiscIO
{

class DriveReader : public SectorReader
{
public:
	static DriveReader* Create(const std::string& drive);
	~DriveReader();
	u64 GetDataSize() const override { return m_size; }
	u64 GetRawSize() const override { return m_size; }

	virtual bool ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8 *out_ptr) override;

private:
	DriveReader(const std::string& drive);
	void GetBlock(u64 block_num, u8 *out_ptr) override;

#ifdef _WIN32
	HANDLE m_disc_handle;
	PREVENT_MEDIA_REMOVAL m_lock_cdrom;
	bool IsOK() { return m_disc_handle != INVALID_HANDLE_VALUE; }
#else
	File::IOFile m_file;
	bool IsOK() { return m_file != nullptr; }
#endif
	s64 m_size;
};

}  // namespace
