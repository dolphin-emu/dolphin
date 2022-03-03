// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/DynamicLibrary.h"

namespace Common
{
#ifdef _WIN32
struct WindowsMemoryRegion;
#endif

// This class lets you create a block of anonymous RAM, and then arbitrarily map views into it.
// Multiple views can mirror the same section of the block, which makes it very convenient for
// emulating memory mirrors.
class MemArena final
{
public:
  MemArena();
  ~MemArena();
  MemArena(const MemArena&) = delete;
  MemArena(MemArena&&) = delete;
  MemArena& operator=(const MemArena&) = delete;
  MemArena& operator=(MemArena&&) = delete;

  ///
  /// Allocate the singular memory segment handled by this MemArena. This will be the actual
  /// 'physical' available memory for this arena. After allocation, it can be interacted with using
  /// CreateView() and ReleaseView(). Used to make a mappable region for emulated memory.
  ///
  /// @param size The amount of bytes that should be allocated in this region.
  ///
  void GrabSHMSegment(size_t size);

  ///
  /// Release the memory segment previously allocated with GrabSHMSegment().
  /// Should not be called before all views have been released.
  ///
  void ReleaseSHMSegment();

  ///
  /// Map a memory region in the memory segment previously allocated with GrabSHMSegment().
  ///
  /// @param offset Offset within the memory segment to map at.
  /// @param size Size of the region to map.
  ///
  /// @return Pointer to the memory region, or nullptr on failure.
  ///
  void* CreateView(s64 offset, size_t size);

  ///
  /// Unmap a memory region previously mapped with CreateView().
  /// Should not be called on a view that is still mapped into the virtual memory region.
  ///
  /// @param view Pointer returned by CreateView().
  /// @param size Size passed to the corresponding CreateView() call.
  ///
  void ReleaseView(void* view, size_t size);

  ///
  /// Reserve the singular 'virtual' memory region handled by this MemArena. This is used to create
  /// our 'fastmem' memory area for the emulated game code to access directly.
  ///
  /// @param memory_size Size in bytes of the memory region to reserve.
  ///
  /// @return Pointer to the memory region, or nullptr on failure.
  ///
  u8* ReserveMemoryRegion(size_t memory_size);

  ///
  /// Release the memory region previously reserved with ReserveMemoryRegion().
  /// Should not be called while any memory region is still mapped.
  ///
  void ReleaseMemoryRegion();

  ///
  /// Map a section from the memory segment previously allocated with GrabSHMSegment()
  /// into the region previously reserved with ReserveMemoryRegion().
  ///
  /// @param offset Offset within the memory segment previous allocated by GrabSHMSegment() to map
  /// from.
  /// @param size Size of the region to map.
  /// @param base Address within the memory region from ReserveMemoryRegion() where to map it.
  ///
  /// @return The address we actually ended up mapping, which should be the given 'base'.
  ///
  void* MapInMemoryRegion(s64 offset, size_t size, void* base);

  ///
  /// Unmap a memory region previously mapped with MapInMemoryRegion().
  ///
  /// @param view Pointer returned by MapInMemoryRegion().
  /// @param size Size passed to the corresponding MapInMemoryRegion() call.
  ///
  void UnmapFromMemoryRegion(void* view, size_t size);

private:
#ifdef _WIN32
  WindowsMemoryRegion* EnsureSplitRegionForMapping(void* address, size_t size);
  bool JoinRegionsAfterUnmap(void* address, size_t size);

  std::vector<WindowsMemoryRegion> m_regions;
  void* m_reserved_region = nullptr;
  void* m_memory_handle = nullptr;
  Common::DynamicLibrary m_kernel32_handle;
  Common::DynamicLibrary m_api_ms_win_core_memory_l1_1_6_handle;
  void* m_address_UnmapViewOfFileEx = nullptr;
  void* m_address_VirtualAlloc2 = nullptr;
  void* m_address_MapViewOfFile3 = nullptr;
#else
#ifdef ANDROID
  int fd;
#else
  int m_shm_fd;
  void* m_reserved_region;
  std::size_t m_reserved_region_size;
#endif
#endif
};

}  // namespace Common
