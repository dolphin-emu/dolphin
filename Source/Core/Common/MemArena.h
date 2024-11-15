// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/DynamicLibrary.h"

namespace Common
{
#ifdef _WIN32
struct WindowsMemoryRegion;

struct WindowsMemoryFunctions
{
  Common::DynamicLibrary m_kernel32_handle;
  Common::DynamicLibrary m_api_ms_win_core_memory_l1_1_6_handle;
  void* m_address_UnmapViewOfFileEx = nullptr;
  void* m_address_VirtualAlloc2 = nullptr;
  void* m_address_MapViewOfFile3 = nullptr;
  void* m_address_VirtualProtect = nullptr;
};
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
  /// @param base_name A base name for the shared memory region, if applicable for this platform.
  /// Will be extended with the process ID.
  ///
  void GrabSHMSegment(size_t size, std::string_view base_name);

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

  ///
  /// Virtual protect a section from the memory region previously mapped by CreateView.
  ///
  /// @param data Pointer to data to protect.
  /// @param size Size of the protection.
  /// @param flag What new permission to protect with.
  ///
  bool VirtualProtectMemoryRegion(u8* data, size_t size, u64 flag);

private:
#ifdef _WIN32
  WindowsMemoryRegion* EnsureSplitRegionForMapping(void* address, size_t size);
  bool JoinRegionsAfterUnmap(void* address, size_t size);

  std::vector<WindowsMemoryRegion> m_regions;
  void* m_reserved_region = nullptr;
  void* m_memory_handle = nullptr;
  WindowsMemoryFunctions m_memory_functions;
#else
  int m_shm_fd = 0;
  void* m_reserved_region = nullptr;
  std::size_t m_reserved_region_size = 0;
#endif
};

// This class represents a single fixed-size memory region where the individual memory pages are
// only actually allocated on first access. The memory will be zero on first access.
class LazyMemoryRegion final
{
public:
  LazyMemoryRegion();
  ~LazyMemoryRegion();
  LazyMemoryRegion(const LazyMemoryRegion&) = delete;
  LazyMemoryRegion(LazyMemoryRegion&&) = delete;
  LazyMemoryRegion& operator=(const LazyMemoryRegion&) = delete;
  LazyMemoryRegion& operator=(LazyMemoryRegion&&) = delete;

  ///
  /// Reserve a memory region.
  ///
  /// @param size The size of the region.
  ///
  /// @return The address the region was mapped at. Returns nullptr on failure.
  ///
  void* Create(size_t size);

  ///
  /// Reset the memory region back to zero, throwing away any mapped pages.
  /// This can only be called after a successful call to Create().
  ///
  void Clear();

  ///
  /// Release the memory previously reserved with Create(). After this call the pointer that was
  /// returned by Create() will become invalid.
  ///
  void Release();

  ///
  /// Ensure that the memory page at the given byte offset from the start of the memory region is
  /// writable. We use this on Windows as a workaround to only actually commit pages as they are
  /// written to. On other OSes this does nothing.
  ///
  /// @param offset The offset into the memory region that should be made writable if it isn't.
  ///
  void EnsureMemoryPageWritable(size_t offset)
  {
#ifdef _WIN32
    const size_t block_index = offset / BLOCK_SIZE;
    if (m_writable_block_handles[block_index] == nullptr)
      MakeMemoryBlockWritable(block_index);
#endif
  }

private:
  void* m_memory = nullptr;
  size_t m_size = 0;

#ifdef _WIN32
  void* m_zero_block = nullptr;
  constexpr static size_t BLOCK_SIZE = 8 * 1024 * 1024;  // size of allocated memory blocks
  WindowsMemoryFunctions m_memory_functions;
  std::vector<void*> m_writable_block_handles;

  void MakeMemoryBlockWritable(size_t offset);
#endif
};

}  // namespace Common
