// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>

#include <minizip/unzip.h>

#include "Common/CommonTypes.h"
#include "Common/ScopeGuard.h"

namespace Common
{
// Reads all of the current file. destination must be big enough to fit the whole file.
template <typename ContiguousContainer>
bool ReadFileFromZip(unzFile file, ContiguousContainer* destination)
{
  const u32 MAX_BUFFER_SIZE = 65535;

  if (unzOpenCurrentFile(file) != UNZ_OK)
    return false;

  Common::ScopeGuard guard{[&] { unzCloseCurrentFile(file); }};

  u32 bytes_to_go = static_cast<u32>(destination->size());
  while (bytes_to_go > 0)
  {
    const int bytes_read =
        unzReadCurrentFile(file, &(*destination)[destination->size() - bytes_to_go],
                           std::min(bytes_to_go, MAX_BUFFER_SIZE));

    if (bytes_read < 0)
      return false;

    bytes_to_go -= static_cast<u32>(bytes_read);
  }

  return true;
}
}  // namespace Common
