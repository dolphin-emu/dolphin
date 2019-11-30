// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdlib>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"

#ifdef _WIN32
#include <windows.h>
#include "Common/StringUtil.h"
#else
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#if defined __APPLE__ || defined __FreeBSD__ || defined __OpenBSD__
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach/vm_param.h>
#endif
#include <sys/sysctl.h>
#elif defined __HAIKU__
#include <OS.h>
#else
#include <sys/sysinfo.h>
#endif
#include <unistd.h>
#endif

namespace Common
{
// This is purposely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(size_t size)
{
#if defined(_WIN32)
  DWORD protection;
#ifndef _WX_EXCLUSIVITY
  protection = PAGE_EXECUTE_READWRITE;
#else
  protection = PAGE_READWRITE;
#endif

  void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT, protection);
#else
  int protection = PROT_READ | PROT_WRITE;
#ifndef _WX_EXCLUSIVITY
  protection |= PROT_EXEC;
#endif

  void* ptr = mmap(nullptr, size, protection, MAP_ANON | MAP_PRIVATE, -1, 0);

  if (ptr == MAP_FAILED)
    ptr = nullptr;
#endif

  if (ptr == nullptr)
    PanicAlert("Failed to allocate executable memory");

  return ptr;
}

void* AllocateMemoryPages(size_t size)
{
#ifdef _WIN32
  void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
#else
  void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

  if (ptr == MAP_FAILED)
    ptr = nullptr;
#endif

  if (ptr == nullptr)
    PanicAlert("Failed to allocate raw memory");

  return ptr;
}

void* AllocateAlignedMemory(size_t size, size_t alignment)
{
#ifdef _WIN32
  void* ptr = _aligned_malloc(size, alignment);
#else
  void* ptr = nullptr;
  if (posix_memalign(&ptr, alignment, size) != 0)
    ERROR_LOG(MEMMAP, "Failed to allocate aligned memory");
#endif

  if (ptr == nullptr)
    PanicAlert("Failed to allocate aligned memory");

  return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
  if (ptr)
  {
#ifdef _WIN32
    if (!VirtualFree(ptr, 0, MEM_RELEASE))
      PanicAlert("FreeMemoryPages failed!\nVirtualFree: %s", GetLastErrorString().c_str());
#else
    if (munmap(ptr, size) != 0)
      PanicAlert("FreeMemoryPages failed!\nmunmap: %s", LastStrerrorString().c_str());
#endif
  }
}

void FreeAlignedMemory(void* ptr)
{
  if (ptr)
  {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
  }
}

void ReadProtectMemory(void* ptr, size_t size)
{
#ifdef _WIN32
  DWORD oldValue;
  if (!VirtualProtect(ptr, size, PAGE_NOACCESS, &oldValue))
    PanicAlert("ReadProtectMemory failed!\nVirtualProtect: %s", GetLastErrorString().c_str());
#else
  if (mprotect(ptr, size, PROT_NONE) != 0)
    PanicAlert("ReadProtectMemory failed!\nmprotect: %s", LastStrerrorString().c_str());
#endif
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
  DWORD oldValue;
  if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, &oldValue))
    PanicAlert("WriteProtectMemory failed!\nVirtualProtect: %s", GetLastErrorString().c_str());
#else
  if (mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ) != 0)
    PanicAlert("WriteProtectMemory failed!\nmprotect: %s", LastStrerrorString().c_str());
#endif
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
  DWORD protection = PAGE_READWRITE;
  if (allowExecute)
  {
#ifndef _WX_EXCLUSIVITY
    protection = PAGE_EXECUTE_READWRITE;
#else
    WARN_LOG(COMMON, "UnWriteProtectMemory: Memory can't be executable and writable simultaneously "
                     "on W^X platform");
#endif
  }

  DWORD oldValue;
  if (!VirtualProtect(ptr, size, protection, &oldValue))
    PanicAlert("UnWriteProtectMemory failed!\nVirtualProtect: %s", GetLastErrorString().c_str());
#else
  int protection = PROT_READ | PROT_WRITE;
  if (allowExecute)
  {
#ifndef _WX_EXCLUSIVITY
    protection |= PROT_EXEC;
#else
    WARN_LOG(COMMON, "UnWriteProtectMemory: Memory can't be executable and writable simultaneously "
                     "on W^X platform");
#endif
  }

  if (mprotect(ptr, size, protection) != 0)
  {
    PanicAlert("UnWriteProtectMemory failed!\nmprotect: %s", LastStrerrorString().c_str());
  }
#endif
}

#ifdef _WX_EXCLUSIVITY
// Used on WX exclusive platforms
bool IsMemoryPageExecutable(void* ptr)
{
#ifdef _WIN32
  MEMORY_BASIC_INFORMATION memory_info;
  if (!VirtualQuery(ptr, &memory_info, sizeof(memory_info)))
  {
    PanicAlert("IsMemoryPageExecutable failed!\nVirtualQuery: %s", GetLastErrorString().c_str());
    return false;
  }

  return (memory_info.Protect & PAGE_EXECUTE_READ) == PAGE_EXECUTE_READ;
#elif defined(__APPLE__)
  vm_address_t address = reinterpret_cast<vm_address_t>(ptr);
  vm_size_t size;
  vm_region_basic_info_data_64_t region_info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  memory_object_name_t object;

  kern_return_t retval =
      vm_region_64(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64,
                   reinterpret_cast<vm_region_info_t>(&region_info), &info_count, &object);
  if (retval != KERN_SUCCESS)
  {
    PanicAlert("IsMemoryPageExecutable failed!\nvm_region_64: 0x%x", retval);
    return false;
  }

  return (region_info.protection & VM_PROT_EXECUTE) == VM_PROT_EXECUTE;
#else
  // There is no POSIX API to get the current protection of a memory page. The commonly suggested
  // workaround for Linux is to parse /proc/self/maps, but this may be too slow for a JIT, and
  // may not work on other platforms (*BSD, for example). A workaround is needed.
#error W^X is not supported on this platform.
#endif
}
#endif

size_t MemPhysical()
{
#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  return memInfo.ullTotalPhys;
#elif defined __APPLE__ || defined __FreeBSD__ || defined __OpenBSD__
  int mib[2];
  size_t physical_memory;
  mib[0] = CTL_HW;
#ifdef __APPLE__
  mib[1] = HW_MEMSIZE;
#elif defined __FreeBSD__
  mib[1] = HW_REALMEM;
#elif defined __OpenBSD__
  mib[1] = HW_PHYSMEM;
#endif
  size_t length = sizeof(size_t);
  sysctl(mib, 2, &physical_memory, &length, NULL, 0);
  return physical_memory;
#elif defined __HAIKU__
  system_info sysinfo;
  get_system_info(&sysinfo);
  return static_cast<size_t>(sysinfo.max_pages * B_PAGE_SIZE);
#else
  struct sysinfo memInfo;
  sysinfo(&memInfo);
  return (size_t)memInfo.totalram * memInfo.mem_unit;
#endif
}

size_t PageSize()
{
#ifdef _WIN32
  SYSTEM_INFO system_info;
  GetNativeSystemInfo(&system_info);

  return system_info.dwPageSize;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

}  // namespace Common
