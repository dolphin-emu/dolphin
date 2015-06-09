// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdio>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/CISOBlob.h"

namespace DiscIO
{

static const char CISO_MAGIC[] = "CISO";

CISOFileReader::CISOFileReader(std::FILE* file)
	: m_file(file)
{
	m_size = m_file.GetSize();

	CISOHeader header;
	m_file.ReadArray(&header, 1);

	m_block_size = header.block_size;

	MapType count = 0;
	for (u32 idx = 0; idx < CISO_MAP_SIZE; ++idx)
		m_ciso_map[idx] = (1 == header.map[idx]) ? count++ : UNUSED_BLOCK_ID;
}

CISOFileReader* CISOFileReader::Create(const std::string& filename)
{
	if (IsCISOBlob(filename))
	{
		File::IOFile f(filename, "rb");
		return new CISOFileReader(f.ReleaseHandle());
	}
	else
	{
		return nullptr;
	}
}

u64 CISOFileReader::GetDataSize() const
{
	return GetRawSize();
}

u64 CISOFileReader::GetRawSize() const
{
	return m_size;
}

bool CISOFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
	while (nbytes != 0)
	{
		u64 const block = offset / m_block_size;
		u64 const data_offset = offset % m_block_size;
		u64 const bytes_to_read = std::min(m_block_size - data_offset, nbytes);

		if (block < CISO_MAP_SIZE && UNUSED_BLOCK_ID != m_ciso_map[block])
		{
			// calculate the base address
			u64 const file_off = CISO_HEADER_SIZE + m_ciso_map[block] * (u64)m_block_size + data_offset;

			if (!(m_file.Seek(file_off, SEEK_SET) && m_file.ReadArray(out_ptr, bytes_to_read)))
			{
				m_file.Clear();
				return false;
			}
		}
		else
		{
			std::fill_n(out_ptr, bytes_to_read, 0);
		}

		out_ptr += bytes_to_read;
		offset  += bytes_to_read;
		nbytes  -= bytes_to_read;
	}

	return true;
}

bool IsCISOBlob(const std::string& filename)
{
	File::IOFile f(filename, "rb");

	CISOHeader header;
	return (f.ReadArray(&header, 1) &&
		std::equal(header.magic, header.magic + sizeof(header.magic), CISO_MAGIC));
}

}  // namespace
