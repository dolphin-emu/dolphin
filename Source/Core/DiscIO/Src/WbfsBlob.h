// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WBFS_BLOB_H
#define _WBFS_BLOB_H

#include "Blob.h"
#include "FileUtil.h"

namespace DiscIO
{

struct wbfs_head_t;

class WbfsFileReader : public IBlobReader
{
	WbfsFileReader(const char* filename);
	~WbfsFileReader();

	bool OpenFiles(const char* filename);	
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

	u64 hd_sector_size;
	u8 hd_sector_shift;
	u32 hd_sector_count;

	u64 wbfs_sector_size;
	u8 wbfs_sector_shift;
	u64 wbfs_sector_count;
	u64 m_disc_info_size;

	u8 disc_table[500];

	u16* m_wlba_table;
	u64 m_blocks_per_disc;

	bool m_good;

public:
	static WbfsFileReader* Create(const char* filename);

	u64 GetDataSize() const { return m_size; }
	u64 GetRawSize() const { return m_size; }
	bool Read(u64 offset, u64 nbytes, u8* out_ptr);
};

bool IsWbfsBlob(const char* filename);


}  // namespace

#endif  // _FILE_BLOB_H
