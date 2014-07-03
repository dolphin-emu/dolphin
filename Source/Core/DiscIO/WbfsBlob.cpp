// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "DiscIO/WbfsBlob.h"

namespace DiscIO
{
static const u64 wii_sector_size = 0x8000;
static const u64 wii_sector_count = 143432 * 2;
static const u64 wii_disc_header_size = 256;

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
	m_files[0]->file.Seek(hd_sector_size + wii_disc_header_size /*+ i * m_disc_info_size*/, SEEK_SET);
	m_files[0]->file.ReadBytes(m_wlba_table, m_blocks_per_disc * sizeof(u16));
}

WbfsFileReader::~WbfsFileReader()
{
	for (u32 i = 0; i != m_files.size(); ++ i)
	{
		delete m_files[i];
	}

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
		if (0 != m_total_files)
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

		m_total_files ++;
		m_files.push_back(new_entry);
	}
}

bool WbfsFileReader::ReadHeader()
{
	m_files[0]->file.Seek(4, SEEK_SET);

	// Read hd size info
	m_files[0]->file.ReadBytes(&hd_sector_count, 4);
	hd_sector_count = Common::swap32(hd_sector_count);

	m_files[0]->file.ReadBytes(&hd_sector_shift, 1);
	hd_sector_size = 1ull << hd_sector_shift;

	if (m_size != hd_sector_count * hd_sector_size)
	{
		//printf("File size doesn't match expected size\n");
		return false;
	}

	// Read wbfs cluster info
	m_files[0]->file.ReadBytes(&wbfs_sector_shift, 1);
	wbfs_sector_size = 1ull << wbfs_sector_shift;
	wbfs_sector_count = m_size / wbfs_sector_size;

	if (wbfs_sector_size < wii_sector_size)
	{
		//Setting this too low would case a very large memory allocation
		return false;
	}

	m_blocks_per_disc = (wii_sector_count * wii_sector_size) / wbfs_sector_size;
	m_disc_info_size = align(wii_disc_header_size + m_blocks_per_disc * 2, hd_sector_size);

	// Read disc table
	m_files[0]->file.Seek(2, SEEK_CUR);
	m_files[0]->file.ReadBytes(disc_table, 500);

	if (0 == disc_table[0])
	{
		//printf("Game must be in 'slot 0'\n");
		return false;
	}

	return true;
}

bool WbfsFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
	while (nbytes)
	{
		u64 read_size = 0;
		File::IOFile& data_file = SeekToCluster(offset, &read_size);
		read_size = (read_size > nbytes) ? nbytes : read_size;

		data_file.ReadBytes(out_ptr, read_size);

		out_ptr += read_size;
		nbytes -= read_size;
		offset += read_size;
	}

	return true;
}

File::IOFile& WbfsFileReader::SeekToCluster(u64 offset, u64* available)
{
	u64 base_cluster = offset >> wbfs_sector_shift;
	if (base_cluster < m_blocks_per_disc)
	{
		u64 cluster_address = wbfs_sector_size * Common::swap16(m_wlba_table[base_cluster]);
		u64 cluster_offset = offset & (wbfs_sector_size - 1);
		u64 final_address = cluster_address + cluster_offset;

		for (u32 i = 0; i != m_total_files; i ++)
		{
			if (final_address < (m_files[i]->base_address + m_files[i]->size))
			{
				m_files[i]->file.Seek(final_address - m_files[i]->base_address, SEEK_SET);
				if (available)
				{
					u64 till_end_of_file = m_files[i]->size - (final_address - m_files[i]->base_address);
					u64 till_end_of_sector = wbfs_sector_size - cluster_offset;
					*available = std::min(till_end_of_file, till_end_of_sector);
				}

				return m_files[i]->file;
			}
		}
	}

	PanicAlert("Read beyond end of disc");
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
