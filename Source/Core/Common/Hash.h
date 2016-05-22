// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/CommonTypes.h"

u32 HashFletcher(const u8* data_u8, size_t length);  // FAST. Length & 1 == 0.
u32 HashAdler32(const u8* data, size_t len);         // Fairly accurate, slightly slower
u32 HashEctor(const u8* ptr, int length);            // JUNK. DO NOT USE FOR NEW THINGS
u64 GetCRC32(const u8* src, u32 len, u32 samples);   // SSE4.2 version of CRC32
u64 GetHashHiresTexture(const u8* src, u32 len, u32 samples = 0);
u64 GetMurmurHash3(const u8* src, u32 len, u32 samples);
u64 GetHash64(const u8* src, u32 len, u32 samples);
void SetHash64Function();
