// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
#ifdef _WIN32
struct WindowsMemoryRegion;
#endif

// This class lets you create a block of anonymous RAM, and then arbitrarily map views into it.
// Multiple views can mirror the same section of the block, which makes it very convenient for
// emulating memory mirrors.
class MemArena
{
public:
  MemArena();
  ~MemArena();
  MemArena(const MemArena&) = delete;
  MemArena(MemArena&&) = delete;
  MemArena& operator=(const MemArena&) = delete;
  MemArena& operator=(MemArena&&) = delete;

  // Allocate a memory segment that can be then interacted with using CreateView() and
  // ReleaseView(). This is used to make a mappable region for emulated memory.
  // 'size' is the amount of bytes available in this region.
  void GrabSHMSegment(size_t size);

  // Release the memory segment previously allocated with GrabSHMSegment().
  void ReleaseSHMSegment();

  // Map a memory region in the memory segment previously allocated with GrabSHMSegment().
  // 'offset': Offset within the memory segment to map at.
  // 'size': Size of the region to map.
  // Returns a pointer to the memory region, or nullptr on failure.
  void* CreateView(s64 offset, size_t size);

  // Unmap a memory region previously mapped with CreateView().
  // 'view': Pointer returned by CreateView().
  // 'size': Size passed to the corresponding CreateView() call.
  void ReleaseView(void* view, size_t size);

  // Reserve a memory region, but don't allocate anything within it.
  // This is used to create our 'fastmem' memory area for the emulated game code to access directly.
  // 'memory_size': Size in bytes of the memory region to reserve.
  // Returns a pointer to the memory region, or nullptr on failure.
  u8* ReserveMemoryRegion(size_t memory_size);

  // Release a memory region previously reserved with ReserveMemoryRegion().
  void ReleaseMemoryRegion();

  // Map a section from the memory segment previously allocated with GrabSHMSegment()
  // into the region previously reserved with ReserveMemoryRegion().
  // 'offset': Offset within the memory segment to map from.
  // 'size': Size of the region to map.
  // 'base': Address where to map it.
  // Returns the address we actually ended up mapping, which should be the given 'base'.
  void* MapInMemoryRegion(s64 offset, size_t size, void* base);

  // Unmap a memory region previously mapped with MapInMemoryRegion().
  // 'view': Pointer returned by MapInMemoryRegion().
  // 'size': Size passed to the corresponding MapInMemoryRegion() call.
  void UnmapFromMemoryRegion(void* view, size_t size);

private:
#ifdef _WIN32
  WindowsMemoryRegion* EnsureSplitRegionForMapping(void* address, size_t size);
  bool JoinRegionsAfterUnmap(void* address, size_t size);

  std::vector<WindowsMemoryRegion> m_regions;
  void* m_reserved_region = nullptr;
  void* m_memory_handle = nullptr;
  void* m_api_ms_win_core_memory_l1_1_6_handle = nullptr;
  void* m_address_VirtualAlloc2 = nullptr;
  void* m_address_MapViewOfFile3 = nullptr;
#else
  int fd;
#endif
};

}  // namespace Common
