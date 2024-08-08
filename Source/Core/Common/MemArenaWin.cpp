// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MemArena.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>

#include <fmt/format.h>

#include <windows.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/DynamicLibrary.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

using PVirtualAlloc2 = PVOID(WINAPI*)(HANDLE Process, PVOID BaseAddress, SIZE_T Size,
                                      ULONG AllocationType, ULONG PageProtection,
                                      MEM_EXTENDED_PARAMETER* ExtendedParameters,
                                      ULONG ParameterCount);

using PMapViewOfFile3 = PVOID(WINAPI*)(HANDLE FileMapping, HANDLE Process, PVOID BaseAddress,
                                       ULONG64 Offset, SIZE_T ViewSize, ULONG AllocationType,
                                       ULONG PageProtection,
                                       MEM_EXTENDED_PARAMETER* ExtendedParameters,
                                       ULONG ParameterCount);

using PUnmapViewOfFileEx = BOOL(WINAPI*)(PVOID BaseAddress, ULONG UnmapFlags);

using PVirtualProtect = BOOL(WINAPI*)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect,
                                      PDWORD lpflOldProtect);

using PIsApiSetImplemented = BOOL(APIENTRY*)(PCSTR Contract);

namespace Common
{
struct WindowsMemoryRegion
{
  u8* m_start;
  size_t m_size;
  bool m_is_mapped;

  WindowsMemoryRegion(u8* start, size_t size, bool is_mapped)
      : m_start(start), m_size(size), m_is_mapped(is_mapped)
  {
  }
};

static bool InitWindowsMemoryFunctions(WindowsMemoryFunctions* functions)
{
  DynamicLibrary kernelBase{"KernelBase.dll"};
  if (!kernelBase.IsOpen())
    return false;

  void* const ptr_IsApiSetImplemented = kernelBase.GetSymbolAddress("IsApiSetImplemented");
  if (!ptr_IsApiSetImplemented)
    return false;
  if (!static_cast<PIsApiSetImplemented>(ptr_IsApiSetImplemented)("api-ms-win-core-memory-l1-1-6"))
    return false;

  functions->m_api_ms_win_core_memory_l1_1_6_handle.Open("api-ms-win-core-memory-l1-1-6.dll");
  functions->m_kernel32_handle.Open("Kernel32.dll");
  if (!functions->m_api_ms_win_core_memory_l1_1_6_handle.IsOpen() ||
      !functions->m_kernel32_handle.IsOpen())
  {
    functions->m_api_ms_win_core_memory_l1_1_6_handle.Close();
    functions->m_kernel32_handle.Close();
    return false;
  }

  void* const address_VirtualAlloc2 =
      functions->m_api_ms_win_core_memory_l1_1_6_handle.GetSymbolAddress("VirtualAlloc2FromApp");
  void* const address_MapViewOfFile3 =
      functions->m_api_ms_win_core_memory_l1_1_6_handle.GetSymbolAddress("MapViewOfFile3FromApp");
  void* const address_UnmapViewOfFileEx =
      functions->m_kernel32_handle.GetSymbolAddress("UnmapViewOfFileEx");
  void* const address_VirtualProtect =
      functions->m_kernel32_handle.GetSymbolAddress("VirtualProtect");
  if (address_VirtualAlloc2 && address_MapViewOfFile3 && address_UnmapViewOfFileEx)
  {
    functions->m_address_VirtualAlloc2 = address_VirtualAlloc2;
    functions->m_address_MapViewOfFile3 = address_MapViewOfFile3;
    functions->m_address_UnmapViewOfFileEx = address_UnmapViewOfFileEx;
    functions->m_address_VirtualProtect = address_VirtualProtect;
    return true;
  }

  // at least one function is not available, use legacy logic
  functions->m_api_ms_win_core_memory_l1_1_6_handle.Close();
  functions->m_kernel32_handle.Close();
  return false;
}

MemArena::MemArena()
{
  // Check if VirtualAlloc2 and MapViewOfFile3 are available, which provide functionality to reserve
  // a memory region no other allocation may occupy while still allowing us to allocate and map
  // stuff within it. If they're not available we'll instead fall back to the 'legacy' logic and
  // just hope that nothing allocates in our address range.
  InitWindowsMemoryFunctions(&m_memory_functions);
}

MemArena::~MemArena()
{
  ReleaseMemoryRegion();
  ReleaseSHMSegment();
}

static DWORD GetHighDWORD(u64 value)
{
  return static_cast<DWORD>(value >> 32);
}

static DWORD GetLowDWORD(u64 value)
{
  return static_cast<DWORD>(value);
}

void MemArena::GrabSHMSegment(size_t size, std::string_view base_name)
{
  const std::string name = fmt::format("{}.{}", base_name, GetCurrentProcessId());
  m_memory_handle =
      CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, GetHighDWORD(size),
                        GetLowDWORD(size), UTF8ToTStr(name).c_str());
}

void MemArena::ReleaseSHMSegment()
{
  if (!m_memory_handle)
    return;
  CloseHandle(m_memory_handle);
  m_memory_handle = nullptr;
}

void* MemArena::CreateView(s64 offset, size_t size)
{
  const u64 off = static_cast<u64>(offset);
  return MapViewOfFileEx(m_memory_handle, FILE_MAP_ALL_ACCESS, GetHighDWORD(off), GetLowDWORD(off),
                         size, nullptr);
}

void MemArena::ReleaseView(void* view, size_t size)
{
  UnmapViewOfFile(view);
}

u8* MemArena::ReserveMemoryRegion(size_t memory_size)
{
  if (m_reserved_region)
  {
    PanicAlertFmt("Tried to reserve a second memory region from the same MemArena.");
    return nullptr;
  }

  u8* base;
  if (m_memory_functions.m_api_ms_win_core_memory_l1_1_6_handle.IsOpen())
  {
    base = static_cast<u8*>(static_cast<PVirtualAlloc2>(m_memory_functions.m_address_VirtualAlloc2)(
        nullptr, nullptr, memory_size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
        nullptr, 0));
    if (base)
    {
      m_reserved_region = base;
      m_regions.emplace_back(base, memory_size, false);
    }
    else
    {
      PanicAlertFmt("Failed to map enough memory space: {}", GetLastErrorString());
    }
  }
  else
  {
    NOTICE_LOG_FMT(MEMMAP, "VirtualAlloc2 and/or MapViewFromFile3 unavailable. "
                           "Falling back to legacy memory mapping.");
    base = static_cast<u8*>(VirtualAlloc(nullptr, memory_size, MEM_RESERVE, PAGE_READWRITE));
    if (base)
      VirtualFree(base, 0, MEM_RELEASE);
    else
      PanicAlertFmt("Failed to find enough memory space: {}", GetLastErrorString());
  }

  return base;
}

void MemArena::ReleaseMemoryRegion()
{
  if (m_memory_functions.m_api_ms_win_core_memory_l1_1_6_handle.IsOpen() && m_reserved_region)
  {
    // user should have unmapped everything by this point, check if that's true and yell if not
    // (it indicates a bug in the emulated memory mapping logic)
    size_t mapped_region_count = 0;
    for (const auto& r : m_regions)
    {
      if (r.m_is_mapped)
        ++mapped_region_count;
    }

    if (mapped_region_count > 0)
    {
      PanicAlertFmt("Error while releasing fastmem region: {} regions are still mapped!",
                    mapped_region_count);
    }

    // then free memory
    VirtualFree(m_reserved_region, 0, MEM_RELEASE);
    m_reserved_region = nullptr;
    m_regions.clear();
  }
}

bool MemArena::VirtualProtectMemoryRegion(u8* data, size_t size, u64 flag)
{
  DWORD lpflOldProtect = 0;
  return static_cast<PVirtualProtect>(m_memory_functions.m_address_VirtualProtect)(
      data, size, flag, &lpflOldProtect);
}

WindowsMemoryRegion* MemArena::EnsureSplitRegionForMapping(void* start_address, size_t size)
{
  u8* const address = static_cast<u8*>(start_address);
  auto& regions = m_regions;
  if (regions.empty())
  {
    NOTICE_LOG_FMT(MEMMAP, "Tried to map a memory region without reserving a memory block first.");
    return nullptr;
  }

  // find closest region that is <= the given address by using upper bound and decrementing
  auto it = std::upper_bound(
      regions.begin(), regions.end(), address,
      [](u8* addr, const WindowsMemoryRegion& region) { return addr < region.m_start; });
  if (it == regions.begin())
  {
    // this should never happen, implies that the given address is before the start of the
    // reserved memory block
    NOTICE_LOG_FMT(MEMMAP, "Invalid address {} given to map.", fmt::ptr(address));
    return nullptr;
  }
  --it;

  if (it->m_is_mapped)
  {
    NOTICE_LOG_FMT(MEMMAP,
                   "Address to map {} with a size of 0x{:x} overlaps with existing mapping "
                   "at {}.",
                   fmt::ptr(address), size, fmt::ptr(it->m_start));
    return nullptr;
  }

  const size_t mapping_index = it - regions.begin();
  u8* const mapping_address = it->m_start;
  const size_t mapping_size = it->m_size;
  if (mapping_address == address)
  {
    // if this region is already split up correctly we don't have to do anything
    if (mapping_size == size)
      return &*it;

    // if this region is smaller than the requested size we can't map
    if (mapping_size < size)
    {
      NOTICE_LOG_FMT(MEMMAP,
                     "Not enough free space at address {} to map 0x{:x} bytes (0x{:x} available).",
                     fmt::ptr(mapping_address), size, mapping_size);
      return nullptr;
    }

    // split region
    if (!VirtualFree(address, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
    {
      NOTICE_LOG_FMT(MEMMAP, "Region splitting failed: {}", GetLastErrorString());
      return nullptr;
    }

    // update tracked mappings and return the first of the two
    it->m_size = size;
    u8* const new_mapping_start = address + size;
    const size_t new_mapping_size = mapping_size - size;
    regions.insert(it + 1, WindowsMemoryRegion(new_mapping_start, new_mapping_size, false));
    return &regions[mapping_index];
  }

  ASSERT(mapping_address < address);

  // is there enough space to map this?
  const size_t size_before = static_cast<size_t>(address - mapping_address);
  const size_t minimum_size = size + size_before;
  if (mapping_size < minimum_size)
  {
    NOTICE_LOG_FMT(MEMMAP,
                   "Not enough free space at address {} to map memory region (need 0x{:x} "
                   "bytes, but only 0x{:x} available).",
                   fmt::ptr(address), minimum_size, mapping_size);
    return nullptr;
  }

  // split region
  if (!VirtualFree(address, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
  {
    NOTICE_LOG_FMT(MEMMAP, "Region splitting failed: {}", GetLastErrorString());
    return nullptr;
  }

  // do we now have two regions or three regions?
  if (mapping_size == minimum_size)
  {
    // split into two; update tracked mappings and return the second one
    it->m_size = size_before;
    u8* const new_mapping_start = address;
    const size_t new_mapping_size = size;
    regions.insert(it + 1, WindowsMemoryRegion(new_mapping_start, new_mapping_size, false));
    return &regions[mapping_index + 1];
  }
  else
  {
    // split into three; update tracked mappings and return the middle one
    it->m_size = size_before;
    u8* const middle_mapping_start = address;
    const size_t middle_mapping_size = size;
    u8* const after_mapping_start = address + size;
    const size_t after_mapping_size = mapping_size - minimum_size;
    regions.insert(it + 1, WindowsMemoryRegion(after_mapping_start, after_mapping_size, false));
    regions.insert(regions.begin() + mapping_index + 1,
                   WindowsMemoryRegion(middle_mapping_start, middle_mapping_size, false));
    return &regions[mapping_index + 1];
  }
}

void* MemArena::MapInMemoryRegion(s64 offset, size_t size, void* base)
{
  if (m_memory_functions.m_api_ms_win_core_memory_l1_1_6_handle.IsOpen())
  {
    WindowsMemoryRegion* const region = EnsureSplitRegionForMapping(base, size);
    if (!region)
    {
      PanicAlertFmt("Splitting memory region failed.");
      return nullptr;
    }

    void* rv = static_cast<PMapViewOfFile3>(m_memory_functions.m_address_MapViewOfFile3)(
        m_memory_handle, nullptr, base, offset, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE,
        nullptr, 0);
    if (rv)
    {
      region->m_is_mapped = true;
    }
    else
    {
      PanicAlertFmt("Mapping memory region failed: {}", GetLastErrorString());

      // revert the split, if any
      JoinRegionsAfterUnmap(base, size);
    }
    return rv;
  }

  return MapViewOfFileEx(m_memory_handle, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
}

bool MemArena::JoinRegionsAfterUnmap(void* start_address, size_t size)
{
  u8* const address = static_cast<u8*>(start_address);
  auto& regions = m_regions;
  if (regions.empty())
  {
    NOTICE_LOG_FMT(MEMMAP,
                   "Tried to unmap a memory region without reserving a memory block first.");
    return false;
  }

  // there should be a mapping that matches the request exactly, find it
  auto it = std::lower_bound(
      regions.begin(), regions.end(), address,
      [](const WindowsMemoryRegion& region, u8* addr) { return region.m_start < addr; });
  if (it == regions.end() || it->m_start != address || it->m_size != size)
  {
    // didn't find it, we were given bogus input
    NOTICE_LOG_FMT(MEMMAP, "Invalid address/size given to unmap.");
    return false;
  }
  it->m_is_mapped = false;

  const bool can_join_with_preceding = it != regions.begin() && !(it - 1)->m_is_mapped;
  const bool can_join_with_succeeding = (it + 1) != regions.end() && !(it + 1)->m_is_mapped;
  if (can_join_with_preceding && can_join_with_succeeding)
  {
    // join three mappings to one
    auto it_preceding = it - 1;
    auto it_succeeding = it + 1;
    const size_t total_size = it_preceding->m_size + size + it_succeeding->m_size;
    if (!VirtualFree(it_preceding->m_start, total_size, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
    {
      NOTICE_LOG_FMT(MEMMAP, "Region coalescing failed: {}", GetLastErrorString());
      return false;
    }

    it_preceding->m_size = total_size;
    regions.erase(it, it + 2);
  }
  else if (can_join_with_preceding && !can_join_with_succeeding)
  {
    // join two mappings to one
    auto it_preceding = it - 1;
    const size_t total_size = it_preceding->m_size + size;
    if (!VirtualFree(it_preceding->m_start, total_size, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
    {
      NOTICE_LOG_FMT(MEMMAP, "Region coalescing failed: {}", GetLastErrorString());
      return false;
    }

    it_preceding->m_size = total_size;
    regions.erase(it);
  }
  else if (!can_join_with_preceding && can_join_with_succeeding)
  {
    // join two mappings to one
    auto it_succeeding = it + 1;
    const size_t total_size = size + it_succeeding->m_size;
    if (!VirtualFree(it->m_start, total_size, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
    {
      NOTICE_LOG_FMT(MEMMAP, "Region coalescing failed: {}", GetLastErrorString());
      return false;
    }

    it->m_size = total_size;
    regions.erase(it_succeeding);
  }
  return true;
}

void MemArena::UnmapFromMemoryRegion(void* view, size_t size)
{
  if (m_memory_functions.m_api_ms_win_core_memory_l1_1_6_handle.IsOpen())
  {
    if (static_cast<PUnmapViewOfFileEx>(m_memory_functions.m_address_UnmapViewOfFileEx)(
            view, MEM_PRESERVE_PLACEHOLDER))
    {
      if (!JoinRegionsAfterUnmap(view, size))
        PanicAlertFmt("Joining memory region failed.");
    }
    else
    {
      PanicAlertFmt("Unmapping memory region failed: {}", GetLastErrorString());
    }
    return;
  }

  UnmapViewOfFile(view);
}

LazyMemoryRegion::LazyMemoryRegion()
{
  InitWindowsMemoryFunctions(&m_memory_functions);
}

LazyMemoryRegion::~LazyMemoryRegion()
{
  Release();
}

void* LazyMemoryRegion::Create(size_t size)
{
  ASSERT(!m_memory);

  if (size == 0)
    return nullptr;

  if (!m_memory_functions.m_api_ms_win_core_memory_l1_1_6_handle.IsOpen())
    return nullptr;

  // reserve block of memory
  const size_t memory_size = Common::AlignUp(size, BLOCK_SIZE);
  const size_t block_count = memory_size / BLOCK_SIZE;
  u8* memory =
      static_cast<u8*>(static_cast<PVirtualAlloc2>(m_memory_functions.m_address_VirtualAlloc2)(
          nullptr, nullptr, memory_size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
          nullptr, 0));
  if (!memory)
  {
    NOTICE_LOG_FMT(MEMMAP, "Memory reservation of {} bytes failed.", size);
    return nullptr;
  }

  // split into individual block-sized regions
  for (size_t i = 0; i < block_count - 1; ++i)
  {
    if (!VirtualFree(memory + i * BLOCK_SIZE, BLOCK_SIZE, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
    {
      NOTICE_LOG_FMT(MEMMAP, "Region splitting failed: {}", GetLastErrorString());

      // release every split block as well as the remaining unsplit one
      for (size_t j = 0; j < i + 1; ++j)
        VirtualFree(memory + j * BLOCK_SIZE, 0, MEM_RELEASE);

      return nullptr;
    }
  }

  m_memory = memory;
  m_size = memory_size;

  // allocate a single block of real memory in the page file
  HANDLE zero_block = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READONLY,
                                        GetHighDWORD(BLOCK_SIZE), GetLowDWORD(BLOCK_SIZE), nullptr);
  if (zero_block == nullptr)
  {
    NOTICE_LOG_FMT(MEMMAP, "CreateFileMapping() failed for zero block: {}", GetLastErrorString());
    Release();
    return nullptr;
  }

  m_zero_block = zero_block;

  // map the zero page into every block
  for (size_t i = 0; i < block_count; ++i)
  {
    void* result = static_cast<PMapViewOfFile3>(m_memory_functions.m_address_MapViewOfFile3)(
        zero_block, nullptr, memory + i * BLOCK_SIZE, 0, BLOCK_SIZE, MEM_REPLACE_PLACEHOLDER,
        PAGE_READONLY, nullptr, 0);
    if (!result)
    {
      NOTICE_LOG_FMT(MEMMAP, "Mapping the zero block failed: {}", GetLastErrorString());
      Release();
      return nullptr;
    }
  }

  m_writable_block_handles.resize(block_count, nullptr);

  return memory;
}

void LazyMemoryRegion::Clear()
{
  ASSERT(m_memory);
  u8* const memory = static_cast<u8*>(m_memory);

  // reset every writable block back to the zero block
  for (size_t i = 0; i < m_writable_block_handles.size(); ++i)
  {
    if (m_writable_block_handles[i] == nullptr)
      continue;

    // unmap the writable block
    if (!static_cast<PUnmapViewOfFileEx>(m_memory_functions.m_address_UnmapViewOfFileEx)(
            memory + i * BLOCK_SIZE, MEM_PRESERVE_PLACEHOLDER))
    {
      PanicAlertFmt("Failed to unmap the writable block: {}", GetLastErrorString());
    }

    // free the writable block
    if (!CloseHandle(m_writable_block_handles[i]))
    {
      PanicAlertFmt("Failed to free the writable block: {}", GetLastErrorString());
    }
    m_writable_block_handles[i] = nullptr;

    // map the zero block
    void* map_result = static_cast<PMapViewOfFile3>(m_memory_functions.m_address_MapViewOfFile3)(
        m_zero_block, nullptr, memory + i * BLOCK_SIZE, 0, BLOCK_SIZE, MEM_REPLACE_PLACEHOLDER,
        PAGE_READONLY, nullptr, 0);
    if (!map_result)
    {
      PanicAlertFmt("Failed to re-map the zero block: {}", GetLastErrorString());
    }
  }
}

void LazyMemoryRegion::Release()
{
  if (m_memory)
  {
    // unmap all pages and release the not-zero block handles
    u8* const memory = static_cast<u8*>(m_memory);
    for (size_t i = 0; i < m_writable_block_handles.size(); ++i)
    {
      static_cast<PUnmapViewOfFileEx>(m_memory_functions.m_address_UnmapViewOfFileEx)(
          memory + i * BLOCK_SIZE, MEM_PRESERVE_PLACEHOLDER);
      if (m_writable_block_handles[i])
      {
        CloseHandle(m_writable_block_handles[i]);
        m_writable_block_handles[i] = nullptr;
      }
    }
  }
  if (m_zero_block)
  {
    CloseHandle(m_zero_block);
    m_zero_block = nullptr;
  }
  if (m_memory)
  {
    u8* const memory = static_cast<u8*>(m_memory);
    const size_t block_count = m_size / BLOCK_SIZE;
    for (size_t i = 0; i < block_count; ++i)
      VirtualFree(memory + i * BLOCK_SIZE, 0, MEM_RELEASE);
    m_memory = nullptr;
    m_size = 0;
  }
}

void LazyMemoryRegion::MakeMemoryBlockWritable(size_t block_index)
{
  u8* const memory = static_cast<u8*>(m_memory);

  // unmap the zero block
  if (!static_cast<PUnmapViewOfFileEx>(m_memory_functions.m_address_UnmapViewOfFileEx)(
          memory + block_index * BLOCK_SIZE, MEM_PRESERVE_PLACEHOLDER))
  {
    PanicAlertFmt("Failed to unmap the zero block: {}", GetLastErrorString());
    return;
  }

  // allocate a fresh block to map
  HANDLE block = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                   GetHighDWORD(BLOCK_SIZE), GetLowDWORD(BLOCK_SIZE), nullptr);
  if (block == nullptr)
  {
    PanicAlertFmt("CreateFileMapping() failed for writable block: {}", GetLastErrorString());
    return;
  }

  // map the new block
  void* map_result = static_cast<PMapViewOfFile3>(m_memory_functions.m_address_MapViewOfFile3)(
      block, nullptr, memory + block_index * BLOCK_SIZE, 0, BLOCK_SIZE, MEM_REPLACE_PLACEHOLDER,
      PAGE_READWRITE, nullptr, 0);
  if (!map_result)
  {
    PanicAlertFmt("Failed to map the writable block: {}", GetLastErrorString());
    CloseHandle(block);
    return;
  }

  m_writable_block_handles[block_index] = block;
}

}  // namespace Common
