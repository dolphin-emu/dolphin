// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <string>

#include <unzip.h>

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

// Packs the directory at directory_path into a new zip file at zip_path.
// If zip_path exists it will be overwritten.
bool PackDirectoryToZip(const std::string& directory_path, const std::string& zip_path);

// Unpacks the zip file at zip_path into the directory at directory_path.
// Existing files may be overwritten if the zip file contains a file with the same relative path.
bool UnpackZipToDirectory(const std::string& zip_path, const std::string& directory_path);
}  // namespace Common
