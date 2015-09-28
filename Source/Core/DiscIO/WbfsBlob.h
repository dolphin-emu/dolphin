// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{

class WbfsFileReader : public IBlobReader
{
public:
	static WbfsFileReader* Create(const std::string& filename);

	bool IsCompressed() const override { return true; }
	u64 GetDataSize() const override { return m_size; }
	u64 GetRawSize() const override { return m_size; }
	bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
	WbfsFileReader(const std::string& filename);
	~WbfsFileReader();

	bool OpenFiles(const std::string& filename);
	bool ReadHeader();

	File::IOFile& SeekToCluster(u64 offset, u64* available);
	bool IsGood() {return m_good;}


	struct file_entry
	{
		File::IOFile file;
		u64 base_address;
		u64 size;
	};

	std::vector<file_entry*> m_files;

	u32 m_total_files;
	u64 m_size;

	u64 m_hd_sector_size;
	u8 m_hd_sector_shift;
	u32 m_hd_sector_count;

	u64 m_wbfs_sector_size;
	u8 m_wbfs_sector_shift;
	u64 m_wbfs_sector_count;
	u64 m_disc_info_size;

	u8 m_disc_table[500];

	u16* m_wlba_table;
	u64 m_blocks_per_disc;

	bool m_good;
};

bool IsWbfsBlob(const std::string& filename);


}  // namespace
