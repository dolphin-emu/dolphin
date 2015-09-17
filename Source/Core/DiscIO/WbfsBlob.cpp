// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "DiscIO/WbfsBlob.h"

namespace DiscIO
{
static const u64 WII_SECTOR_SIZE = 0x8000;
static const u64 WII_SECTOR_COUNT = 143432 * 2;
static const u64 WII_DISC_HEADER_SIZE = 256;

static inline u64 align(u64 value, u64 bounds)
{
	return (value + (bounds - 1)) & (~(bounds - 1));
}

WbfsFileReader::WbfsFileReader(const std::string& filename)
	: m_total_files(0), m_size(0), m_wlba_table(nullptr), m_good(true)
{
	if (filename.length() < 4 || !OpenFiles(filename) || !ReadHeader())
	{
		m_good = false;
		return;
	}

	// Grab disc info (assume slot 0, checked in ReadHeader())
	m_wlba_table = new u16[m_blocks_per_disc];
	m_files[0]->file.Seek(m_hd_sector_size + WII_DISC_HEADER_SIZE /*+ i * m_disc_info_size*/, SEEK_SET);
	m_files[0]->file.ReadBytes(m_wlba_table, m_blocks_per_disc * sizeof(u16));
	for (size_t i = 0; i < m_blocks_per_disc; i++)
		m_wlba_table[i] = Common::swap16(m_wlba_table[i]);
}

WbfsFileReader::~WbfsFileReader()
{
	for (file_entry* entry : m_files)
		delete entry;

	delete[] m_wlba_table;
}

bool WbfsFileReader::OpenFiles(const std::string& filename)
{
	m_total_files = 0;

	while (true)
	{
		file_entry* new_entry = new file_entry;

		// Replace last character with index (e.g. wbfs = wbf1)
		std::string path = filename;
		if (m_total_files != 0)
		{
			path[path.length() - 1] = '0' + m_total_files;
		}

		if (!new_entry->file.Open(path, "rb"))
		{
			delete new_entry;
			return 0 != m_total_files;
		}

		new_entry->base_address = m_size;
		new_entry->size = new_entry->file.GetSize();
		m_size += new_entry->size;

		m_total_files++;
		m_files.push_back(new_entry);
	}
}

bool WbfsFileReader::ReadHeader()
{
	// Read hd size info
	m_files[0]->file.ReadBytes(&m_header, sizeof(WbfsHeader));
	m_header.hd_sector_count = Common::swap32(m_header.hd_sector_count);

	m_hd_sector_size = 1ull << m_header.hd_sector_shift;

	if (m_size != (m_header.hd_sector_count * m_hd_sector_size))
		return false;

	// Read wbfs cluster info
	m_wbfs_sector_size = 1ull << m_header.wbfs_sector_shift;
	m_wbfs_sector_count = m_size / m_wbfs_sector_size;

	if (m_wbfs_sector_size < WII_SECTOR_SIZE)
		return false;

	m_blocks_per_disc = (WII_SECTOR_COUNT * WII_SECTOR_SIZE) / m_wbfs_sector_size;
	m_disc_info_size = align(WII_DISC_HEADER_SIZE + m_blocks_per_disc * sizeof(u16), m_hd_sector_size);

	return m_header.disc_table[0] != 0;
}

bool WbfsFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
	while (nbytes)
	{
		u64 read_size;
		File::IOFile& data_file = SeekToCluster(offset, &read_size);
		if (read_size == 0)
			return false;
		read_size = std::min(read_size, nbytes);

		if (!data_file.ReadBytes(out_ptr, read_size))
		{
			data_file.Clear();
			return false;
		}

		out_ptr += read_size;
		nbytes -= read_size;
		offset += read_size;
	}

	return true;
}

File::IOFile& WbfsFileReader::SeekToCluster(u64 offset, u64* available)
{
	u64 base_cluster = (offset >> m_header.wbfs_sector_shift);
	if (base_cluster < m_blocks_per_disc)
	{
		u64 cluster_address = m_wbfs_sector_size * m_wlba_table[base_cluster];
		u64 cluster_offset = offset & (m_wbfs_sector_size - 1);
		u64 final_address = cluster_address + cluster_offset;

		for (u32 i = 0; i != m_total_files; i++)
		{
			if (final_address < (m_files[i]->base_address + m_files[i]->size))
			{
				m_files[i]->file.Seek(final_address - m_files[i]->base_address, SEEK_SET);
				if (available)
				{
					u64 till_end_of_file = m_files[i]->size - (final_address - m_files[i]->base_address);
					u64 till_end_of_sector = m_wbfs_sector_size - cluster_offset;
					*available = std::min(till_end_of_file, till_end_of_sector);
				}

				return m_files[i]->file;
			}
		}
	}

	PanicAlert("Read beyond end of disc");
	if (available)
		*available = 0;
	m_files[0]->file.Seek(0, SEEK_SET);
	return m_files[0]->file;
}

WbfsFileReader* WbfsFileReader::Create(const std::string& filename)
{
	WbfsFileReader* reader = new WbfsFileReader(filename);

	if (reader->IsGood())
	{
		return reader;
	}
	else
	{
		delete reader;
		return nullptr;
	}
}

bool IsWbfsBlob(const std::string& filename)
{
	File::IOFile f(filename, "rb");

	u8 magic[4] = {0, 0, 0, 0};
	f.ReadBytes(&magic, 4);

	return (magic[0] == 'W') &&
	       (magic[1] == 'B') &&
	       (magic[2] == 'F') &&
	       (magic[3] == 'S');
}

}  // namespace
