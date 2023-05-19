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

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/ashmem.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

namespace Common
{
#define ASHMEM_DEVICE "/dev/ashmem"

static int AshmemCreateFileMapping(const char* name, size_t size)
{
  // ASharedMemory path - works on API >= 26 and falls through on API < 26:

  // We can't call ASharedMemory_create the normal way without increasing the
  // minimum version requirement to API 26, so we use dlopen/dlsym instead
  static void* libandroid = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
  static auto shared_memory_create =
      reinterpret_cast<int (*)(const char*, size_t)>(dlsym(libandroid, "ASharedMemory_create"));
  if (shared_memory_create)
    return shared_memory_create(name, size);

  // /dev/ashmem path - works on API < 29:

  int fd, ret;
  fd = open(ASHMEM_DEVICE, O_RDWR);
  if (fd < 0)
    return fd;

  // We don't really care if we can't set the name, it is optional
  ioctl(fd, ASHMEM_SET_NAME, name);

  ret = ioctl(fd, ASHMEM_SET_SIZE, size);
  if (ret < 0)
  {
    close(fd);
    NOTICE_LOG_FMT(MEMMAP, "Ashmem returned error: {:#010x}", ret);
    return ret;
  }
  return fd;
}

MemArena::MemArena() = default;
MemArena::~MemArena() = default;

void MemArena::GrabSHMSegment(size_t size, std::string_view base_name)
{
  const std::string name = fmt::format("{}.{}", base_name, getpid());
  m_shm_fd = AshmemCreateFileMapping(name.c_str(), size);
  if (m_shm_fd < 0)
    NOTICE_LOG_FMT(MEMMAP, "Ashmem allocation failed");
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
  // Android 4.3 changed how mmap works.
  // if we map it private and then munmap it, we can't use the base returned.
  // This may be due to changes in them to support a full SELinux implementation.
  const int flags = MAP_ANON | MAP_SHARED;
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
  void* retval = mmap(view, size, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (retval == MAP_FAILED)
    NOTICE_LOG_FMT(MEMMAP, "mmap failed");
}
}  // namespace Common
