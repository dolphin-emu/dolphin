// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>

#include <mz_compat.h>

#include "Common/CommonTypes.h"
#include "Common/ScopeGuard.h"

namespace Common
{
// Reads all of the current file. destination must be big enough to fit the whole file.
inline bool ReadFileFromZip(unzFile file, u8* destination, u64 len)
{
  const u64 MAX_BUFFER_SIZE = 65535;

  if (unzOpenCurrentFile(file) != UNZ_OK)
    return false;

  Common::ScopeGuard guard{[&] { unzCloseCurrentFile(file); }};

  u64 bytes_to_go = len;
  while (bytes_to_go > 0)
  {
    // NOTE: multiples of 4G can't cause read_len == 0 && bytes_to_go > 0, as MAX_BUFFER_SIZE is
    // small.
    const u32 read_len = static_cast<u32>(std::min(bytes_to_go, MAX_BUFFER_SIZE));
    const int rv = unzReadCurrentFile(file, destination, read_len);
    if (rv < 0)
      return false;

    const u32 bytes_read = static_cast<u32>(rv);
    bytes_to_go -= bytes_read;
    destination += bytes_read;
  }

  return unzEndOfFile(file) == 1;
}

template <typename ContiguousContainer>
bool ReadFileFromZip(unzFile file, ContiguousContainer* destination)
{
  return ReadFileFromZip(file, reinterpret_cast<u8*>(destination->data()), destination->size());
}
}  // namespace Common
