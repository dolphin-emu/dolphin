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

#ifndef _CISO_BLOB_H
#define _CISO_BLOB_H

#include "Blob.h"

#include <cstdio>

#define CISO_MAGIC     "CISO"
#define CISO_HEAD_SIZE (0x8000)
#define CISO_MAP_SIZE  (CISO_HEAD_SIZE - 8)

namespace DiscIO
{

bool IsCISOBlob(const char* filename);

// Blocks that won't compress to less than 97% of the original size are stored as-is.
typedef struct CISO_Head_t
{
   u8  magic[4];            // "CISO"
   u32 block_size;          // stored as litte endian (not network byte order)
   u8  map[CISO_MAP_SIZE];  // 0=unused, 1=used, others=invalid
} CISO_Head_t;

typedef u16 CISO_Map_t;

const CISO_Map_t CISO_UNUSED_BLOCK = (CISO_Map_t)~0;

class CISOFileReader : public IBlobReader
{
	FILE* file_;
	CISOFileReader(FILE* file__);
	s64 size;

public:
	static CISOFileReader* Create(const char* filename);
	~CISOFileReader();
	u64 GetDataSize() const { return size; }
	u64 GetRawSize() const { return size; }
	bool Read(u64 offset, u64 nbytes, u8* out_ptr);

private:
	CISO_Head_t header;
	CISO_Map_t ciso_map[CISO_MAP_SIZE];
};

}  // namespace

#endif  // _FILE_BLOB_H
