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
MemArena::MemArena() = default;
MemArena::~MemArena() = default;

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

void* MemArena::CreateView(s64 offset, size_t size)
{
  return MapInMemoryRegion(offset, size, nullptr);
}

void MemArena::ReleaseView(void* view, size_t size)
{
  UnmapFromMemoryRegion(view, size);
}

u8* MemArena::ReserveMemoryRegion(size_t memory_size)
{
  u8* base = static_cast<u8*>(VirtualAlloc(nullptr, memory_size, MEM_RESERVE, PAGE_READWRITE));
  if (!base)
  {
    PanicAlertFmt("Failed to map enough memory space: {}", GetLastErrorString());
    return nullptr;
  }
  VirtualFree(base, 0, MEM_RELEASE);
  return base;
}

void MemArena::ReleaseMemoryRegion()
{
}

void* MemArena::MapInMemoryRegion(s64 offset, size_t size, void* base)
{
  return MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
}

void MemArena::UnmapFromMemoryRegion(void* view, size_t size)
{
  UnmapViewOfFile(view);
}
}  // namespace Common
