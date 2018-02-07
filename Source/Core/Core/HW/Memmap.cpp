// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// NOTE:
// These functions are primarily used by the interpreter versions of the LoadStore instructions.
// However, if a JITed instruction (for example lwz) wants to access a bad memory area that call
// may be redirected here (for example to Read_U32()).

#include "Core/HW/Memmap.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemArena.h"
#include "Common/MemoryUtil.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/MemoryInterface.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WII_IPC.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/PixelEngine.h"

namespace Memory
{
// =================================
// Init() declarations
// ----------------
// Store the MemArena here
u8* physical_base = nullptr;
u8* logical_base = nullptr;

// The MemArena class
static MemArena g_arena;
// ==============

// STATE_TO_SAVE
static bool m_IsInitialized = false;  // Save the Init(), Shutdown() state
// END STATE_TO_SAVE

u8* m_pRAM;
u8* m_pL1Cache;
u8* m_pEXRAM;
u8* m_pFakeVMEM;

// MMIO mapping object.
std::unique_ptr<MMIO::Mapping> mmio_mapping;

static std::unique_ptr<MMIO::Mapping> InitMMIO()
{
  auto mmio = std::make_unique<MMIO::Mapping>();

  CommandProcessor::RegisterMMIO(mmio.get(), 0x0C000000);
  PixelEngine::RegisterMMIO(mmio.get(), 0x0C001000);
  VideoInterface::RegisterMMIO(mmio.get(), 0x0C002000);
  ProcessorInterface::RegisterMMIO(mmio.get(), 0x0C003000);
  MemoryInterface::RegisterMMIO(mmio.get(), 0x0C004000);
  DSP::RegisterMMIO(mmio.get(), 0x0C005000);
  DVDInterface::RegisterMMIO(mmio.get(), 0x0C006000);
  SerialInterface::RegisterMMIO(mmio.get(), 0x0C006400);
  ExpansionInterface::RegisterMMIO(mmio.get(), 0x0C006800);
  AudioInterface::RegisterMMIO(mmio.get(), 0x0C006C00);

  return mmio;
}

static std::unique_ptr<MMIO::Mapping> InitMMIOWii()
{
  auto mmio = InitMMIO();

  IOS::RegisterMMIO(mmio.get(), 0x0D000000);
  DVDInterface::RegisterMMIO(mmio.get(), 0x0D006000);
  SerialInterface::RegisterMMIO(mmio.get(), 0x0D006400);
  ExpansionInterface::RegisterMMIO(mmio.get(), 0x0D006800);
  AudioInterface::RegisterMMIO(mmio.get(), 0x0D006C00);

  return mmio;
}

bool IsInitialized()
{
  return m_IsInitialized;
}

struct PhysicalMemoryRegion
{
  u8** out_pointer;
  u32 physical_address;
  u32 size;
  enum : u32
  {
    ALWAYS = 0,
    FAKE_VMEM = 1,
    WII_ONLY = 2,
  } flags;
  u32 shm_position;
};

struct LogicalMemoryView
{
  void* mapped_pointer;
  u32 mapped_size;
  u32 logical_address;
  u32 physical_offset;
};

// Dolphin allocates memory to represent four regions:
// - 32MB RAM (actually 24MB on hardware), available on Gamecube and Wii
// - 64MB "EXRAM", RAM only available on Wii
// - 32MB FakeVMem, allocated in GameCube mode when MMU support is turned off.
//   This is used to approximate the behavior of a common library which pages
//   memory to and from the DSP's dedicated RAM. The DSP's RAM (ARAM) isn't
//   directly addressable on GameCube.
// - 256KB Locked L1, to represent cache lines allocated out of the L1 data
//   cache in Locked L1 mode.  Dolphin does not emulate this hardware feature
//   accurately; it just pretends there is extra memory at 0xE0000000.
//
// The 4GB starting at physical_base represents access from the CPU
// with address translation turned off. (This is only used by the CPU;
// other devices, like the GPU, use other rules, approximated by
// Memory::GetPointer.) This memory is laid out as follows:
// [0x00000000, 0x02000000) - 32MB RAM
// [0x02000000, 0x08000000) - Mirrors of 32MB RAM
// [0x08000000, 0x0C000000) - EFB "mapping" (not handled here)
// [0x0C000000, 0x0E000000) - MMIO etc. (not handled here)
// [0x10000000, 0x14000000) - 64MB RAM (Wii-only; slightly slower)
// [0x7E000000, 0x80000000) - FakeVMEM
// [0xE0000000, 0xE0040000) - 256KB locked L1
//
// The 4GB starting at logical_base represents access from the CPU
// with address translation turned on.  This mapping is computed based
// on the BAT registers.
//
// Each of these 4GB regions is followed by 4GB of empty space so overflows
// in address computation in the JIT don't access the wrong memory.
//
// The neighboring mirrors of RAM ([0x02000000, 0x08000000), etc.) exist because
// the bus masks off the bits in question for RAM accesses; using them is a
// terrible idea because the CPU cache won't handle them correctly, but a
// few buggy games (notably Rogue Squadron 2) use them by accident. They
// aren't backed by memory mappings because they are used very rarely.
//
// Dolphin doesn't emulate the difference between cached and uncached access.
//
// TODO: The actual size of RAM is REALRAM_SIZE (24MB); the other 8MB shouldn't
// be backed by actual memory.
static PhysicalMemoryRegion physical_regions[] = {
    {&m_pRAM, 0x00000000, RAM_SIZE, PhysicalMemoryRegion::ALWAYS},
    {&m_pL1Cache, 0xE0000000, L1_CACHE_SIZE, PhysicalMemoryRegion::ALWAYS},
    {&m_pFakeVMEM, 0x7E000000, FAKEVMEM_SIZE, PhysicalMemoryRegion::FAKE_VMEM},
    {&m_pEXRAM, 0x10000000, EXRAM_SIZE, PhysicalMemoryRegion::WII_ONLY},
};

static std::vector<LogicalMemoryView> logical_mapped_entries;

void Init()
{
  bool wii = SConfig::GetInstance().bWii;
  bool bMMU = SConfig::GetInstance().bMMU;
  bool bFakeVMEM = false;
#ifndef _ARCH_32
  // If MMU is turned off in GameCube mode, turn on fake VMEM hack.
  // The fake VMEM hack's address space is above the memory space that we
  // allocate on 32bit targets, so disable it there.
  bFakeVMEM = !wii && !bMMU;
#endif

  u32 flags = 0;
  if (wii)
    flags |= PhysicalMemoryRegion::WII_ONLY;
  if (bFakeVMEM)
    flags |= PhysicalMemoryRegion::FAKE_VMEM;
  u32 mem_size = 0;
  for (PhysicalMemoryRegion& region : physical_regions)
  {
    if ((flags & region.flags) != region.flags)
      continue;
    region.shm_position = mem_size;
    mem_size += region.size;
  }
  g_arena.GrabSHMSegment(mem_size);
  physical_base = MemArena::FindMemoryBase();

  for (PhysicalMemoryRegion& region : physical_regions)
  {
    if ((flags & region.flags) != region.flags)
      continue;

    u8* base = physical_base + region.physical_address;
    *region.out_pointer = (u8*)g_arena.CreateView(region.shm_position, region.size, base);

    if (!*region.out_pointer)
    {
      PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
      exit(0);
    }
  }

#ifndef _ARCH_32
  logical_base = physical_base + 0x200000000;
#endif

  if (wii)
    mmio_mapping = InitMMIOWii();
  else
    mmio_mapping = InitMMIO();

  SetDCacheEmulationEnabled(SConfig::GetInstance().bDCache);
  Clear();

  INFO_LOG(MEMMAP, "Memory system initialized. RAM at %p", m_pRAM);
  m_IsInitialized = true;
}

void UpdateLogicalMemory(const PowerPC::BatTable& dbat_table)
{
  for (auto& entry : logical_mapped_entries)
  {
    g_arena.ReleaseView(entry.mapped_pointer, entry.mapped_size);
  }
  logical_mapped_entries.clear();
  for (u32 i = 0; i < dbat_table.size(); ++i)
  {
    if (dbat_table[i] & PowerPC::BAT_PHYSICAL_BIT)
    {
      u32 logical_address = i << PowerPC::BAT_INDEX_SHIFT;
      // TODO: Merge adjacent mappings to make this faster.
      u32 logical_size = PowerPC::BAT_PAGE_SIZE;
      u32 translated_address = dbat_table[i] & PowerPC::BAT_RESULT_MASK;
      for (const auto& physical_region : physical_regions)
      {
        u32 mapping_address = physical_region.physical_address;
        u32 mapping_end = mapping_address + physical_region.size;
        u32 intersection_start = std::max(mapping_address, translated_address);
        u32 intersection_end = std::min(mapping_end, translated_address + logical_size);
        if (intersection_start < intersection_end)
        {
          // Found an overlapping region; map it.
          u32 physical_offset = intersection_start - mapping_address;
          u32 position = physical_region.shm_position + physical_offset;
          u8* base = logical_base + logical_address + intersection_start - translated_address;
          u32 mapped_size = intersection_end - intersection_start;

          void* mapped_pointer = g_arena.CreateView(position, mapped_size, base);
          if (!mapped_pointer)
          {
            PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
            exit(0);
          }
          u32 la = logical_address + intersection_start - translated_address;
          logical_mapped_entries.push_back({mapped_pointer, mapped_size, la, physical_offset});
        }
      }
    }
  }
}

void DoState(PointerWrap& p)
{
  bool wii = SConfig::GetInstance().bWii;
  p.DoArray(m_pRAM, RAM_SIZE);
  p.DoArray(m_pL1Cache, L1_CACHE_SIZE);
  p.DoMarker("Memory RAM");
  if (m_pFakeVMEM)
    p.DoArray(m_pFakeVMEM, FAKEVMEM_SIZE);
  p.DoMarker("Memory FakeVMEM");
  if (wii)
    p.DoArray(m_pEXRAM, EXRAM_SIZE);
  p.DoMarker("Memory EXRAM");
}

void Shutdown()
{
  m_IsInitialized = false;
  u32 flags = 0;
  if (SConfig::GetInstance().bWii)
    flags |= PhysicalMemoryRegion::WII_ONLY;
  if (m_pFakeVMEM)
    flags |= PhysicalMemoryRegion::FAKE_VMEM;
  for (PhysicalMemoryRegion& region : physical_regions)
  {
    if ((flags & region.flags) != region.flags)
      continue;
    g_arena.ReleaseView(*region.out_pointer, region.size);
    *region.out_pointer = nullptr;
  }
  for (auto& entry : logical_mapped_entries)
  {
    g_arena.ReleaseView(entry.mapped_pointer, entry.mapped_size);
  }
  logical_mapped_entries.clear();
  g_arena.ReleaseSHMSegment();
  physical_base = nullptr;
  logical_base = nullptr;
  mmio_mapping.reset();
  INFO_LOG(MEMMAP, "Memory system shut down.");
}

void Clear()
{
  if (m_pRAM)
    memset(m_pRAM, 0, RAM_SIZE);
  if (m_pL1Cache)
    memset(m_pL1Cache, 0, L1_CACHE_SIZE);
  if (m_pFakeVMEM)
    memset(m_pFakeVMEM, 0, FAKEVMEM_SIZE);
  if (m_pEXRAM)
    memset(m_pEXRAM, 0, EXRAM_SIZE);
}

static inline u8* GetPointerForRange(u32 address, size_t size)
{
  // Make sure we don't have a range spanning 2 separate banks
  if (size >= EXRAM_SIZE)
    return nullptr;

  // Check that the beginning and end of the range are valid
  u8* pointer = GetPointer(address);
  if (!pointer || !GetPointer(address + u32(size) - 1))
    return nullptr;

  return pointer;
}

void CopyFromEmu(void* data, u32 address, size_t size)
{
  if (size == 0)
    return;

  void* pointer = GetPointerForRange(address, size);
  if (!pointer)
  {
    PanicAlert("Invalid range in CopyFromEmu. %zx bytes from 0x%08x", size, address);
    return;
  }
  memcpy(data, pointer, size);
}

void CopyToEmu(u32 address, const void* data, size_t size)
{
  if (size == 0)
    return;

  void* pointer = GetPointerForRange(address, size);
  if (!pointer)
  {
    PanicAlert("Invalid range in CopyToEmu. %zx bytes to 0x%08x", size, address);
    return;
  }
  memcpy(pointer, data, size);
}

void Memset(u32 address, u8 value, size_t size)
{
  if (size == 0)
    return;

  void* pointer = GetPointerForRange(address, size);
  if (!pointer)
  {
    PanicAlert("Invalid range in Memset. %zx bytes at 0x%08x", size, address);
    return;
  }
  memset(pointer, value, size);
}

std::string GetString(u32 em_address, size_t size)
{
  const char* ptr = reinterpret_cast<const char*>(GetPointer(em_address));
  if (ptr == nullptr)
    return "";

  if (size == 0)  // Null terminated string.
  {
    return std::string(ptr);
  }
  else  // Fixed size string, potentially null terminated or null padded.
  {
    size_t length = strnlen(ptr, size);
    return std::string(ptr, length);
  }
}

u8* GetPointer(u32 address)
{
  // TODO: Should we be masking off more bits here?  Can all devices access
  // EXRAM?
  address &= 0x3FFFFFFF;
  if (address < REALRAM_SIZE)
    return m_pRAM + address;

  if (m_pEXRAM)
  {
    if ((address >> 28) == 0x1 && (address & 0x0fffffff) < EXRAM_SIZE)
      return m_pEXRAM + (address & EXRAM_MASK);
  }

  PanicAlert("Unknown Pointer 0x%08x PC 0x%08x LR 0x%08x", address, PC, LR);

  return nullptr;
}

u8 Read_U8(u32 address)
{
  return *GetPointer(address);
}

u16 Read_U16(u32 address)
{
  return Common::swap16(GetPointer(address));
}

u32 Read_U32(u32 address)
{
  return Common::swap32(GetPointer(address));
}

u64 Read_U64(u32 address)
{
  return Common::swap64(GetPointer(address));
}

void Write_U8(u8 value, u32 address)
{
  *GetPointer(address) = value;
}

void Write_U16(u16 value, u32 address)
{
  u16 swapped_value = Common::swap16(value);
  std::memcpy(GetPointer(address), &swapped_value, sizeof(u16));
}

void Write_U32(u32 value, u32 address)
{
  u32 swapped_value = Common::swap32(value);
  std::memcpy(GetPointer(address), &swapped_value, sizeof(u32));
}

void Write_U64(u64 value, u32 address)
{
  u64 swapped_value = Common::swap64(value);
  std::memcpy(GetPointer(address), &swapped_value, sizeof(u64));
}

void Write_U32_Swap(u32 value, u32 address)
{
  std::memcpy(GetPointer(address), &value, sizeof(u32));
}

void Write_U64_Swap(u64 value, u32 address)
{
  std::memcpy(GetPointer(address), &value, sizeof(u64));
}

using PhysicalAddressType = u32;

template <typename T>
static bool Overlaps(T a_addr, size_t a_len, T b_addr, size_t b_len)
{
  T a1 = a_addr;
  T a2 = a1 + static_cast<T>(a_len);
  T b1 = b_addr;
  T b2 = a2 + static_cast<T>(b_len);

  return !((a1 >= b2) || (b1 >= a2));
}

static std::optional<PhysicalAddressType> GetPhysicalAddressForHostAddress(uintptr_t host_address)
{
  // physical addresses
  for (const PhysicalMemoryRegion& region : physical_regions)
  {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(*region.out_pointer);
    if (Overlaps(host_address, 1, ptr, region.size))
      return (region.physical_address + (host_address - ptr)) & 0x3FFFFFFF;
  }

  // fastmem/logical base
  for (const LogicalMemoryView& view : logical_mapped_entries)
  {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(view.mapped_pointer);
    if (Overlaps(host_address, 1, ptr, view.mapped_size))
      return (view.physical_offset + (host_address - ptr)) & 0x3FFFFFFF;
  }

  return {};
}

enum class PageLockState
{
  ReadWrite,
  ReadOnly,
  NoAccess
};

// Indexed by physical_address+length.
// This way we can use lower_bound() to identify overlapping locks.
using LockMap = std::multimap<PhysicalAddressType, Lock::SharedPtr>;
static LockMap s_locks_by_physical_addr;
static std::mutex s_lock_map_mutex;
static bool s_dcache_emulation = false;

bool GetDCacheEmulationEnabled()
{
  return s_dcache_emulation;
}

void SetDCacheEmulationEnabled(bool enabled)
{
  std::lock_guard<std::mutex> guard(s_lock_map_mutex);
  if (s_dcache_emulation == enabled)
    return;

  // Flush all locks upon changing mode.
  FlushAllLocks(LockAccessType::Write);
  s_dcache_emulation = enabled;
}

static void UpdateHostPageState(PhysicalAddressType phys_addr, u32 size, PageLockState state)
{
  auto DoPages = [phys_addr, size, state](u32 region_physical_address, u32 region_size,
                                          void* region_base_ptr, u32 region_virt_addr) {
    if ((phys_addr + size) <= region_physical_address)
      return;

    u32 offset_into_region;
    u32 size_in_region;
    if (region_physical_address < phys_addr)
    {
      offset_into_region = phys_addr - region_physical_address;
      if (offset_into_region >= region_size)
        return;
      size_in_region = std::min(region_size - offset_into_region, size);
    }
    else
    {
      offset_into_region = 0;
      size_in_region = std::min(region_size, size - (region_physical_address - phys_addr));
    }

    u32 page_size = static_cast<u32>(Common::MemPageSize());
    uintptr_t page_mask = ~static_cast<uintptr_t>(page_size - 1);
    uintptr_t offset_base_ptr = reinterpret_cast<uintptr_t>(region_base_ptr) + offset_into_region;
    uintptr_t start_page = (offset_base_ptr & page_mask);
    uintptr_t end_page = ((offset_base_ptr + (size_in_region - 1)) & page_mask);

    // WARN_LOG(VIDEO, "Physical %08X Virtual %08X Offset %u Overlapping %u", phys_addr,
    // region_virt_addr, offset_into_region, size_in_region);

    void* prot_ptr = reinterpret_cast<void*>(start_page);
    size_t prot_size = end_page - start_page + page_size;
    switch (state)
    {
    case PageLockState::ReadWrite:
      Common::UnWriteProtectMemory(prot_ptr, prot_size, false);
      // WARN_LOG(VIDEO, "R/W Physical %08X Virtual %p Size %u", phys_addr, prot_ptr,
      // (u32)prot_size);
      break;
    case PageLockState::ReadOnly:
      Common::WriteProtectMemory(prot_ptr, prot_size, false);
      // WARN_LOG(VIDEO, "R/O Physical %08X Virtual %p Size %u", phys_addr, prot_ptr,
      // (u32)prot_size);
      break;
    case PageLockState::NoAccess:
      Common::ReadProtectMemory(prot_ptr, prot_size);
      // WARN_LOG(VIDEO, "N/A Physical %08X Virtual %p Size %u", phys_addr, prot_ptr,
      // (u32)prot_size);
      break;
    }
  };

  // physical addresses
  for (PhysicalMemoryRegion& region : physical_regions)
    DoPages(region.physical_address, region.size, *region.out_pointer, 0);

  // fastmem/logical base
  for (const LogicalMemoryView& view : logical_mapped_entries)
    DoPages(view.physical_offset, view.mapped_size, view.mapped_pointer, view.logical_address);
}

static std::optional<std::pair<LockMap::iterator, LockMap::iterator>>
GetOverlappingLocks(PhysicalAddressType address, u32 size)
{
  PhysicalAddressType physical_end = address + size;

  auto start = s_locks_by_physical_addr.lower_bound(address);
  if (start == s_locks_by_physical_addr.end() || start->second->guest_address > physical_end)
    return {};

  auto next = start;
  auto end = ++next;
  while (next != s_locks_by_physical_addr.end() && next->second->guest_address < physical_end)
    end = ++next;

  return std::make_pair(start, end);
}

static void UpdateGuestPageState(PhysicalAddressType address, u32 size, PageLockState to_state)
{
  auto SkipLockedRegion = [to_state](LockType type) {
    switch (type)
    {
    case LockType::Read:
      // Read locks are the highest level of locking, so we can't raise the protection any higher.
      return true;
    case LockType::Write:
      // Write locks are only outclassed by read locks.
      return to_state != PageLockState::NoAccess;
    default:
      return true;
    }
  };

  auto overlapping_locks = GetOverlappingLocks(address, size);
  if (!overlapping_locks)
  {
    // Nothing overlapping, so just update the whole region.
    UpdateHostPageState(address, size, to_state);
    return;
  }

  // Basically, we look for locks which lie either after, or overlap with the range we're
  // updating. If there is a gap between the range and the lock, we modify that range's
  // protection, and move the range forward. If there is an overlap, the overlapped segment
  // is either skipped if no update is needed, otherwise it is modified, and the range
  // incremented as before.
  // TODO: Improve this explanation.
  while (size > 0)
  {
    u32 modify_size = size;
    for (auto iter = overlapping_locks->first; iter != overlapping_locks->second; iter++)
    {
      const Lock* lock = iter->second.get();

      if (address < lock->guest_address)
      {
        // Case A: |mem   |locked region|
        modify_size = std::min(modify_size, lock->guest_address - address);
      }
      else
      {
        // Case B: |locked region|   mem
        u32 overlap_start = address - lock->guest_address;
        if (overlap_start >= lock->length)
        {
          // mem is past the locked region
          continue;
        }

        u32 covered_size = std::min(lock->length - overlap_start, size);
        if (SkipLockedRegion(lock->type))
        {
          // No need to change the overlap.
          address += covered_size;
          size -= covered_size;
          modify_size = 0;
          break;
        }
        else
        {
          // Still need to update this overlapped area.
          modify_size = std::min(modify_size, covered_size);
        }
      }
    }

    if (modify_size > 0)
    {
      UpdateHostPageState(address, modify_size, to_state);
      address += modify_size;
      size -= modify_size;
    }
  }
}

Lock::SharedPtr CreateLock(u32 address, u32 length, LockType type, void* userdata,
                           const Lock::Callback& callback)
{
  _assert_(length > 0);
  // WARN_LOG(VIDEO, "NEW_LOCK(%08X, %u)", address, length);

  // Align the guest address to the host page size.
  // This way, when multiple locks exist which occupy the same host pages, we do not need
  // to repeatedly change the protection on host pages, saving expensive system calls.
  const u32 page_size = static_cast<u32>(Common::MemPageSize());
  const u32 page_offset_mask = page_size - 1;
  const u32 page_mask = ~page_offset_mask;
  length = (length + (address & page_offset_mask) + (page_size - 1)) & page_mask;
  address &= page_mask;

  Lock::SharedPtr lock = std::make_shared<Lock>();
  lock->guest_address = address;
  lock->length = length;
  lock->userdata = userdata;
  lock->type = type;
  lock->callback = callback;

  std::lock_guard<std::mutex> guard(s_lock_map_mutex);

  if (!s_dcache_emulation)
  {
    PageLockState new_state =
        (type == LockType::Read) ? PageLockState::NoAccess : PageLockState::ReadOnly;
    UpdateGuestPageState(address, length, new_state);
  }

  s_locks_by_physical_addr.emplace(lock->guest_address + lock->length, lock);
  return lock;
}

static void InternalRemoveLock(Lock::SharedPtr& lock)
{
  // WARN_LOG(VIDEO, "REMOVE_LOCK(%08X, %u)", lock->guest_address, lock->length);
  auto it_range = s_locks_by_physical_addr.equal_range(lock->guest_address + lock->length);
  for (auto it = it_range.first; it != it_range.second; it++)
  {
    if (it->second == lock)
    {
      s_locks_by_physical_addr.erase(it);
      break;
    }
  }

  if (!s_dcache_emulation)
    UpdateGuestPageState(lock->guest_address, lock->length, PageLockState::ReadWrite);
}

void RemoveLock(Lock::SharedPtr& lock)
{
  std::lock_guard<std::mutex> guard(s_lock_map_mutex);
  InternalRemoveLock(lock);
}

bool HandlePageFault(uintptr_t host_address)
{
  if (s_dcache_emulation)
    return false;

  size_t page_size = Common::MemPageSize();
  uintptr_t page_mask = ~static_cast<uintptr_t>(page_size - 1);
  uintptr_t host_page_address = host_address & page_mask;

  auto fault_physical_address = GetPhysicalAddressForHostAddress(host_page_address);
  // WARN_LOG(VIDEO, "Host fault %p guest physical %08X", host_address, fault_physical_address ?
  // *fault_physical_address : 0);
  if (!fault_physical_address)
    return false;

  // If we faulted on a page that is read-protected, it could have been either a read or
  // a write. For the purposes of the callback, we assume it to be a write, as we would
  // not be faulting on reading a write-protected page.
  // TODO: Use the operating system exception code here to better detect the access type.
  LockAccessType access_type = LockAccessType::Write;
  return FlushLocksInPhysicalRange(*fault_physical_address, static_cast<u32>(page_size),
                                   access_type);
}

void FlushAllLocks(LockAccessType access_type)
{
  s_lock_map_mutex.lock();

  while (!s_locks_by_physical_addr.empty())
  {
    auto temp = s_locks_by_physical_addr.begin()->second;
    InternalRemoveLock(temp);

    s_lock_map_mutex.unlock();
    temp->callback(temp, access_type);
    s_lock_map_mutex.lock();
  }

  s_lock_map_mutex.unlock();
}

// Must be called with the mutex held.
static bool InternalFlushLocksInPhysicalRange(u32 start_address, u32 length,
                                              LockAccessType access_type)
{
  auto iter = s_locks_by_physical_addr.lower_bound(start_address);
  if (iter == s_locks_by_physical_addr.end())
    return false;

  bool result = false;
  u32 max_address = start_address + length;
  while (iter != s_locks_by_physical_addr.end() && iter->second->guest_address <= max_address)
  {
    Lock::SharedPtr& lock = iter->second;

    // Check for an overlap between this lock and the provided range.
    if (!Overlaps(start_address, length, lock->guest_address, length))
    {
      iter++;
      continue;
    }

    // With dcache emulation on, the below is a noop, so remove the lock.
    Lock::SharedPtr temp = lock;
    InternalRemoveLock(temp);

    // Flush all overlapping locks.
    // This is needed because otherwise we'll fault during the handler.
    // Faulting during the handler can be both slower (due to the extra context switch),
    // or cause issues (e.g. EFB copies with async events cannot be re-entrant).
    if (!s_dcache_emulation)
      InternalFlushLocksInPhysicalRange(temp->guest_address, temp->length, access_type);

    // Now run the callbacks.
    // We don't want to hold the lock map lock while executing the handler.
    // Cross-thread handlers could deadlock otherwise, e.g. CPU thread faults,
    // video thread gets kicked to run, a previous opcode creates a new lock while
    // the CPU thread still holds the mutex.
    s_lock_map_mutex.unlock();
    temp->callback(temp, access_type);
    s_lock_map_mutex.lock();

    // iter may be invalidated by the above callback.
    iter = s_locks_by_physical_addr.lower_bound(start_address);
    result = true;
  }

  return result;
}

bool FlushLocksInPhysicalRange(u32 start_address, u32 length, LockAccessType access_type)
{
  s_lock_map_mutex.lock();
  bool result = InternalFlushLocksInPhysicalRange(start_address, length, access_type);
  s_lock_map_mutex.unlock();
  return result;
}

void FlushOverlappingLocks(const Lock* lock, LockAccessType access_type)
{
  FlushLocksInPhysicalRange(lock->guest_address, lock->length, access_type);
}

}  // namespace
