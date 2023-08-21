// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/Filesystem.h"

namespace DiscIO
{
FileInfo::~FileInfo() = default;

u64 FileInfo::GetTotalSize() const
{
  if (!IsDirectory())
    return GetSize();

  u64 size = 0;

  for (const auto& entry : *this)
    size += entry.GetTotalSize();

  return size;
}

FileSystem::~FileSystem() = default;

}  // namespace DiscIO
