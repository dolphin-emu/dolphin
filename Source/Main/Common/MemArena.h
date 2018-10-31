// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Common/CommonTypes.h"

namespace Common
{
// This class lets you create a block of anonymous RAM, and then arbitrarily map views into it.
// Multiple views can mirror the same section of the block, which makes it very convenient for
// emulating
// memory mirrors.
class MemArena
{
public:
  void GrabSHMSegment(size_t size);
  void ReleaseSHMSegment();
  void* CreateView(s64 offset, size_t size, void* base = nullptr);
  void ReleaseView(void* view, size_t size);

  // This finds 1 GB in 32-bit, 16 GB in 64-bit.
  static u8* FindMemoryBase();

private:
#ifdef _WIN32
  HANDLE hMemoryMapping;
#else
  int fd;
#endif
};

}  // namespace Common
