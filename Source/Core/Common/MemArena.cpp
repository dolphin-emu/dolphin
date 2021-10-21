// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <cstdlib>
#include <set>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemArena.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef ANDROID
#include <dlfcn.h>
#include <linux/ashmem.h>
#include <sys/ioctl.h>
#endif
#endif

namespace Common
{
#ifdef ANDROID
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
#endif

void MemArena::GrabSHMSegment(size_t size)
{
#ifdef _WIN32
  const std::string name = "dolphin-emu." + std::to_string(GetCurrentProcessId());
  hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                     static_cast<DWORD>(size), UTF8ToTStr(name).c_str());
#elif defined(ANDROID)
  fd = AshmemCreateFileMapping(("dolphin-emu." + std::to_string(getpid())).c_str(), size);
  if (fd < 0)
  {
    NOTICE_LOG_FMT(MEMMAP, "Ashmem allocation failed");
    return;
  }
#else
  const std::string file_name = "/dolphin-emu." + std::to_string(getpid());
  fd = shm_open(file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd == -1)
  {
    ERROR_LOG_FMT(MEMMAP, "shm_open failed: {}", strerror(errno));
    return;
  }
  shm_unlink(file_name.c_str());
  if (ftruncate(fd, size) < 0)
    ERROR_LOG_FMT(MEMMAP, "Failed to allocate low memory space");
#endif
}

void MemArena::ReleaseSHMSegment()
{
#ifdef _WIN32
  CloseHandle(hMemoryMapping);
  hMemoryMapping = 0;
#else
  close(fd);
#endif
}

void* MemArena::CreateView(s64 offset, size_t size, void* base)
{
#ifdef _WIN32
  return MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
#else
  void* retval = mmap(base, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | ((base == nullptr) ? 0 : MAP_FIXED), fd, offset);

  if (retval == MAP_FAILED)
  {
    NOTICE_LOG_FMT(MEMMAP, "mmap failed");
    return nullptr;
  }
  else
  {
    return retval;
  }
#endif
}

void MemArena::ReleaseView(void* view, size_t size)
{
#ifdef _WIN32
  UnmapViewOfFile(view);
#else
  munmap(view, size);
#endif
}

u8* MemArena::FindMemoryBase()
{
#if _ARCH_32
  const size_t memory_size = 0x31000000;
#else
  const size_t memory_size = 0x400000000;
#endif

#ifdef _WIN32
  u8* base = static_cast<u8*>(VirtualAlloc(nullptr, memory_size, MEM_RESERVE, PAGE_READWRITE));
  if (!base)
  {
    PanicAlertFmt("Failed to map enough memory space: {}", GetLastErrorString());
    return nullptr;
  }
  VirtualFree(base, 0, MEM_RELEASE);
  return base;
#else
#ifdef ANDROID
  // Android 4.3 changed how mmap works.
  // if we map it private and then munmap it, we can't use the base returned.
  // This may be due to changes in them support a full SELinux implementation.
  const int flags = MAP_ANON | MAP_SHARED;
#else
  const int flags = MAP_ANON | MAP_PRIVATE;
#endif
  void* base = mmap(nullptr, memory_size, PROT_NONE, flags, -1, 0);
  if (base == MAP_FAILED)
  {
    PanicAlertFmt("Failed to map enough memory space: {}", LastStrerrorString());
    return nullptr;
  }
  munmap(base, memory_size);
  return static_cast<u8*>(base);
#endif
}

}  // namespace Common
