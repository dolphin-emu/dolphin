// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <zlib.h>

#include "Common/CRC32.h"

namespace Common
{
u32 ComputeCRC32(std::string_view data)
{
  // Use zlibs crc32 implementation to compute the hash
  u32 hash = crc32(0L, Z_NULL, 0);
  hash = crc32(hash, (const Bytef*)data.data(), (u32)data.size());
  return hash;
}
}  // namespace Common
