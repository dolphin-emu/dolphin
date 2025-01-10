// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
u32 HashAdler32(const u8* data, size_t len);
// JUNK. DO NOT USE FOR NEW THINGS
u32 HashEctor(const u8* data, size_t len);

// Specialized hash function used for the texture cache
u64 GetHash64(const u8* src, u32 len, u32 samples);

u32 StartCRC32();
u32 UpdateCRC32(u32 crc, const u8* data, size_t len);
u32 ComputeCRC32(const u8* data, size_t len);
u32 ComputeCRC32(std::string_view data);

// For SD card emulation
u8 HashCrc7(const u8* ptr, size_t length);
u16 HashCrc16(const u8* ptr, size_t length);

template <size_t N>
u8 HashCrc7(const std::array<u8, N>& data)
{
  return HashCrc7(data.data(), N);
}
template <size_t N>
u16 HashCrc16(const std::array<u8, N>& data)
{
  return HashCrc16(data.data(), N);
}
}  // namespace Common
