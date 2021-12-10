// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CRC32.h"

#include <zlib.h>

namespace Common
{
u32 ComputeCRC32(std::string_view data)
{
  const Bytef* buf = reinterpret_cast<const Bytef*>(data.data());
  uInt len = static_cast<uInt>(data.size());
  // Use zlibs crc32 implementation to compute the hash
  u32 hash = crc32(0L, Z_NULL, 0);
  hash = crc32(hash, buf, len);
  return hash;
}
}  // namespace Common
