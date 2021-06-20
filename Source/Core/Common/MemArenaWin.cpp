// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MemArena.h"

#include <cstddef>
#include <cstdlib>
#include <set>
#include <string>

#include <windows.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

namespace Common
{
void MemArena::GrabSHMSegment(size_t size)
{
  const std::string name = "dolphin-emu." + std::to_string(GetCurrentProcessId());
  hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                     static_cast<DWORD>(size), UTF8ToTStr(name).c_str());
}

void MemArena::ReleaseSHMSegment()
{
  CloseHandle(hMemoryMapping);
  hMemoryMapping = 0;
}

void* MemArena::CreateView(s64 offset, size_t size, void* base)
{
  return MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
}

void MemArena::ReleaseView(void* view, size_t size)
{
  UnmapViewOfFile(view);
}

u8* MemArena::FindMemoryBase()
{
#if _ARCH_32
  const size_t memory_size = 0x31000000;
#else
  const size_t memory_size = 0x400000000;
#endif

  u8* base = static_cast<u8*>(VirtualAlloc(nullptr, memory_size, MEM_RESERVE, PAGE_READWRITE));
  if (!base)
  {
    PanicAlertFmt("Failed to map enough memory space: {}", GetLastErrorString());
    return nullptr;
  }
  VirtualFree(base, 0, MEM_RELEASE);
  return base;
}
}  // namespace Common
