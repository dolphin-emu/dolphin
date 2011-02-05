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

#ifndef _HASH_H_
#define _HASH_H_

#include "Common.h"

u32 HashFletcher(const u8* data_u8, size_t length);  // FAST. Length & 1 == 0.
u32 HashAdler32(const u8* data, size_t len);         // Fairly accurate, slightly slower
u32 HashFNV(const u8* ptr, int length);              // Another fast and decent hash
u32 HashEctor(const u8* ptr, int length);            // JUNK. DO NOT USE FOR NEW THINGS
u64 GetCRC32(const u8 *src, int len, u32 samples); // SSE4.2 version of CRC32
u64 GetHashHiresTexture(const u8 *src, int len, u32 samples);
u64 GetMurmurHash3(const u8 *src, int len, u32 samples);
u64 GetHash64(const u8 *src, int len, u32 samples);
void SetHash64Function(bool useHiresTextures);
#endif // _HASH_H_
