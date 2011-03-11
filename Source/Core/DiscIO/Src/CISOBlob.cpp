// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Blob.h"
#include "CISOBlob.h"

namespace DiscIO
{

CISOFileReader::CISOFileReader(std::FILE* file)
	: m_file(file)
{
	m_size = m_file.GetSize();

	memset(&header, 0, sizeof(header));
	m_file.ReadArray(&header, 1);

	CISO_Map_t count = 0;
	int idx;
	for (idx = 0; idx < CISO_MAP_SIZE; idx++)
		ciso_map[idx] = (header.map[idx] == 1) ? count++ : CISO_UNUSED_BLOCK;
}

CISOFileReader* CISOFileReader::Create(const char* filename)
{
	if (IsCISOBlob(filename))
	{
		File::IOFile f(filename, "rb");
		return new CISOFileReader(f.ReleaseHandle());
	}
	else
		return NULL;
}

bool CISOFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
	u64 bytesRead = 0;
	while (bytesRead < nbytes)
	{
		u32 block = (u32)(offset / header.block_size);
		u32 data_offset = offset % header.block_size;
		u32 bytes_to_read = (u32)min((u64)(header.block_size - data_offset), nbytes);
		if ((block >= CISO_MAP_SIZE) || (ciso_map[block] == CISO_UNUSED_BLOCK))
		{
			memset(out_ptr, 0, bytes_to_read);
			out_ptr += bytes_to_read;
			offset += bytes_to_read;
			bytesRead += bytes_to_read;
		}
		else
		{
			// calcualte the base address
			u64 file_off = CISO_HEAD_SIZE + ciso_map[block] * (u64)header.block_size + data_offset;

			if (!(m_file.Seek(file_off, SEEK_SET) && m_file.ReadBytes(out_ptr, bytes_to_read)))
				return false;

			out_ptr += bytes_to_read;
			offset += bytes_to_read;
			bytesRead += bytes_to_read;
		}
	}
	return true;
}

bool IsCISOBlob(const char* filename)
{
	File::IOFile f(filename, "rb");

	CISO_Head_t header;
	return (f.ReadArray(&header, 1) && (memcmp(header.magic, CISO_MAGIC, sizeof(header.magic)) == 0));
}

}  // namespace
