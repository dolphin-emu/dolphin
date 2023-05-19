// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MemArena.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include <fmt/format.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

namespace Common
{
MemArena::MemArena() = default;
MemArena::~MemArena() = default;

void MemArena::GrabSHMSegment(size_t size, std::string_view base_name)
{
  const std::string file_name = fmt::format("/{}.{}", base_name, getpid());
  m_shm_fd = shm_open(file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (m_shm_fd == -1)
  {
    ERROR_LOG_FMT(MEMMAP, "shm_open failed: {}", strerror(errno));
    return;
  }
  shm_unlink(file_name.c_str());
  if (ftruncate(m_shm_fd, size) < 0)
    ERROR_LOG_FMT(MEMMAP, "Failed to allocate low memory space");
}

void MemArena::ReleaseSHMSegment()
{
  close(m_shm_fd);
}

void* MemArena::CreateView(s64 offset, size_t size)
{
  void* retval = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, offset);
  if (retval == MAP_FAILED)
  {
    NOTICE_LOG_FMT(MEMMAP, "mmap failed");
    return nullptr;
  }
  else
  {
    return retval;
  }
}

void MemArena::ReleaseView(void* view, size_t size)
{
  munmap(view, size);
}

u8* MemArena::ReserveMemoryRegion(size_t memory_size)
{
  const int flags = MAP_ANON | MAP_PRIVATE;
  void* base = mmap(nullptr, memory_size, PROT_NONE, flags, -1, 0);
  if (base == MAP_FAILED)
  {
    PanicAlertFmt("Failed to map enough memory space: {}", LastStrerrorString());
    return nullptr;
  }
  m_reserved_region = base;
  m_reserved_region_size = memory_size;
  return static_cast<u8*>(base);
}

void MemArena::ReleaseMemoryRegion()
{
  if (m_reserved_region)
  {
    munmap(m_reserved_region, m_reserved_region_size);
    m_reserved_region = nullptr;
  }
}

void* MemArena::MapInMemoryRegion(s64 offset, size_t size, void* base)
{
  void* retval = mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, m_shm_fd, offset);
  if (retval == MAP_FAILED)
  {
    NOTICE_LOG_FMT(MEMMAP, "mmap failed");
    return nullptr;
  }
  else
  {
    return retval;
  }
}

void MemArena::UnmapFromMemoryRegion(void* view, size_t size)
{
  void* retval = mmap(view, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (retval == MAP_FAILED)
    NOTICE_LOG_FMT(MEMMAP, "mmap failed");
}
}  // namespace Common
