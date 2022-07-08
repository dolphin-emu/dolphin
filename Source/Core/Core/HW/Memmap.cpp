// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// NOTE:
// These functions are primarily used by the interpreter versions of the LoadStore instructions.
// However, if a JITed instruction (for example lwz) wants to access a bad memory area that call
// may be redirected here (for example to Read_U32()).

#include "Core/HW/Memmap.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <tuple>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemArena.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
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
#include "Core/System.h"
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
u8* physical_page_mappings_base = nullptr;
u8* logical_page_mappings_base = nullptr;
static bool is_fastmem_arena_initialized = false;

// The MemArena class
static Common::MemArena g_arena;
// ==============

// STATE_TO_SAVE
static bool m_IsInitialized = false;  // Save the Init(), Shutdown() state
// END STATE_TO_SAVE

u8* m_pRAM;
u8* m_pL1Cache;
u8* m_pEXRAM;
u8* m_pFakeVMEM;

// s_ram_size is the amount allocated by the emulator, whereas s_ram_size_real
// is what will be reported in lowmem, and thus used by emulated software.
// Note: Writing to lowmem is done by IPL. If using retail IPL, it will
// always be set to 24MB.
static u32 s_ram_size_real;
static u32 s_ram_size;
static u32 s_ram_mask;
static u32 s_fakevmem_size;
static u32 s_fakevmem_mask;
static u32 s_L1_cache_size;
static u32 s_L1_cache_mask;
static u32 s_io_size;
// s_exram_size is the amount allocated by the emulator, whereas s_exram_size_real
// is what gets used by emulated software.  If using retail IOS, it will
// always be set to 64MB.
static u32 s_exram_size_real;
static u32 s_exram_size;
static u32 s_exram_mask;

u32 GetRamSizeReal()
{
  return s_ram_size_real;
}
u32 GetRamSize()
{
  return s_ram_size;
}
u32 GetRamMask()
{
  return s_ram_mask;
}
u32 GetFakeVMemSize()
{
  return s_fakevmem_size;
}
u32 GetFakeVMemMask()
{
  return s_fakevmem_mask;
}
u32 GetL1CacheSize()
{
  return s_L1_cache_size;
}
u32 GetL1CacheMask()
{
  return s_L1_cache_mask;
}
u32 GetIOSize()
{
  return s_io_size;
}
u32 GetExRamSizeReal()
{
  return s_exram_size_real;
}
u32 GetExRamSize()
{
  return s_exram_size;
}
u32 GetExRamMask()
{
  return s_exram_mask;
}

// MMIO mapping object.
std::unique_ptr<MMIO::Mapping> mmio_mapping;

static void InitMMIO(bool is_wii)
{
  mmio_mapping = std::make_unique<MMIO::Mapping>();

  CommandProcessor::RegisterMMIO(mmio_mapping.get(), 0x0C000000);
  PixelEngine::RegisterMMIO(mmio_mapping.get(), 0x0C001000);
  VideoInterface::RegisterMMIO(mmio_mapping.get(), 0x0C002000);
  ProcessorInterface::RegisterMMIO(mmio_mapping.get(), 0x0C003000);
  MemoryInterface::RegisterMMIO(mmio_mapping.get(), 0x0C004000);
  DSP::RegisterMMIO(mmio_mapping.get(), 0x0C005000);
  DVDInterface::RegisterMMIO(mmio_mapping.get(), 0x0C006000);
  SerialInterface::RegisterMMIO(mmio_mapping.get(), 0x0C006400);
  ExpansionInterface::RegisterMMIO(mmio_mapping.get(), 0x0C006800);
  AudioInterface::RegisterMMIO(mmio_mapping.get(), 0x0C006C00);
  if (is_wii)
  {
    IOS::RegisterMMIO(mmio_mapping.get(), 0x0D000000);
    DVDInterface::RegisterMMIO(mmio_mapping.get(), 0x0D006000);
    SerialInterface::RegisterMMIO(mmio_mapping.get(), 0x0D006400);
    ExpansionInterface::RegisterMMIO(mmio_mapping.get(), 0x0D006800);
    AudioInterface::RegisterMMIO(mmio_mapping.get(), 0x0D006C00);
  }
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
  bool active;
};

struct LogicalMemoryView
{
  void* mapped_pointer;
  u32 mapped_size;
};

// Dolphin allocates memory to represent four regions:
// - 32MB RAM (actually 24MB on hardware), available on GameCube and Wii
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
// [0x02000000, 0x08000000) - Mirrors of 32MB RAM (not handled here)
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
// TODO: The actual size of RAM is 24MB; the other 8MB shouldn't be backed by actual memory.
// TODO: Do we want to handle the mirrors of the GC RAM?
static std::array<PhysicalMemoryRegion, 4> s_physical_regions;

static std::vector<LogicalMemoryView> logical_mapped_entries;

static std::array<void*, PowerPC::BAT_PAGE_COUNT> s_physical_page_mappings;
static std::array<void*, PowerPC::BAT_PAGE_COUNT> s_logical_page_mappings;

void Init()
{
  const auto get_mem1_size = [] {
    if (Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE))
      return Config::Get(Config::MAIN_MEM1_SIZE);
    return Memory::MEM1_SIZE_RETAIL;
  };
  const auto get_mem2_size = [] {
    if (Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE))
      return Config::Get(Config::MAIN_MEM2_SIZE);
    return Memory::MEM2_SIZE_RETAIL;
  };
  s_ram_size_real = get_mem1_size();
  s_ram_size = MathUtil::NextPowerOf2(GetRamSizeReal());
  s_ram_mask = GetRamSize() - 1;
  s_fakevmem_size = 0x02000000;
  s_fakevmem_mask = GetFakeVMemSize() - 1;
  s_L1_cache_size = 0x00040000;
  s_L1_cache_mask = GetL1CacheSize() - 1;
  s_io_size = 0x00010000;
  s_exram_size_real = get_mem2_size();
  s_exram_size = MathUtil::NextPowerOf2(GetExRamSizeReal());
  s_exram_mask = GetExRamSize() - 1;

  s_physical_regions[0] = {&m_pRAM, 0x00000000, GetRamSize(), PhysicalMemoryRegion::ALWAYS, false};
  s_physical_regions[1] = {&m_pL1Cache, 0xE0000000, GetL1CacheSize(), PhysicalMemoryRegion::ALWAYS,
                           false};
  s_physical_regions[2] = {&m_pFakeVMEM, 0x7E000000, GetFakeVMemSize(),
                           PhysicalMemoryRegion::FAKE_VMEM, false};
  s_physical_regions[3] = {&m_pEXRAM, 0x10000000, GetExRamSize(), PhysicalMemoryRegion::WII_ONLY,
                           false};

  const bool wii = SConfig::GetInstance().bWii;
  const bool mmu = Core::System::GetInstance().IsMMUMode();

  bool fake_vmem = false;
#ifndef _ARCH_32
  // If MMU is turned off in GameCube mode, turn on fake VMEM hack.
  // The fake VMEM hack's address space is above the memory space that we
  // allocate on 32bit targets, so disable it there.
  fake_vmem = !wii && !mmu;
#endif

  u32 mem_size = 0;
  for (PhysicalMemoryRegion& region : s_physical_regions)
  {
    if (!wii && (region.flags & PhysicalMemoryRegion::WII_ONLY))
      continue;
    if (!fake_vmem && (region.flags & PhysicalMemoryRegion::FAKE_VMEM))
      continue;

    region.shm_position = mem_size;
    region.active = true;
    mem_size += region.size;
  }
  g_arena.GrabSHMSegment(mem_size);

  s_physical_page_mappings.fill(nullptr);

  // Create an anonymous view of the physical memory
  for (const PhysicalMemoryRegion& region : s_physical_regions)
  {
    if (!region.active)
      continue;

    *region.out_pointer = (u8*)g_arena.CreateView(region.shm_position, region.size);

    if (!*region.out_pointer)
    {
      PanicAlertFmt(
          "Memory::Init(): Failed to create view for physical region at 0x{:08X} (size 0x{:08X}).",
          region.physical_address, region.size);
      exit(0);
    }

    for (u32 i = 0; i < region.size; i += PowerPC::BAT_PAGE_SIZE)
    {
      const size_t index = (i + region.physical_address) >> PowerPC::BAT_INDEX_SHIFT;
      s_physical_page_mappings[index] = *region.out_pointer + i;
    }
  }

  physical_page_mappings_base = reinterpret_cast<u8*>(s_physical_page_mappings.data());
  logical_page_mappings_base = reinterpret_cast<u8*>(s_logical_page_mappings.data());

  InitMMIO(wii);

  Clear();

  INFO_LOG_FMT(MEMMAP, "Memory system initialized. RAM at {}", fmt::ptr(m_pRAM));
  m_IsInitialized = true;
}

bool InitFastmemArena()
{
#if _ARCH_32
  const size_t memory_size = 0x31000000;
#else
  const size_t memory_size = 0x400000000;
#endif
  physical_base = g_arena.ReserveMemoryRegion(memory_size);

  if (!physical_base)
  {
    PanicAlertFmt("Memory::InitFastmemArena(): Failed finding a memory base.");
    return false;
  }

  for (const PhysicalMemoryRegion& region : s_physical_regions)
  {
    if (!region.active)
      continue;

    u8* base = physical_base + region.physical_address;
    u8* view = (u8*)g_arena.MapInMemoryRegion(region.shm_position, region.size, base);

    if (base != view)
    {
      PanicAlertFmt("Memory::InitFastmemArena(): Failed to map memory region at 0x{:08X} "
                    "(size 0x{:08X}) into physical fastmem region.",
                    region.physical_address, region.size);
      return false;
    }
  }

#ifndef _ARCH_32
  logical_base = physical_base + 0x200000000;
#endif

  is_fastmem_arena_initialized = true;
  return true;
}

void UpdateLogicalMemory(const PowerPC::BatTable& dbat_table)
{
  for (auto& entry : logical_mapped_entries)
  {
    g_arena.UnmapFromMemoryRegion(entry.mapped_pointer, entry.mapped_size);
  }
  logical_mapped_entries.clear();

  s_logical_page_mappings.fill(nullptr);

  for (u32 i = 0; i < dbat_table.size(); ++i)
  {
    if (dbat_table[i] & PowerPC::BAT_PHYSICAL_BIT)
    {
      u32 logical_address = i << PowerPC::BAT_INDEX_SHIFT;
      // TODO: Merge adjacent mappings to make this faster.
      u32 logical_size = PowerPC::BAT_PAGE_SIZE;
      u32 translated_address = dbat_table[i] & PowerPC::BAT_RESULT_MASK;
      for (const auto& physical_region : s_physical_regions)
      {
        if (!physical_region.active)
          continue;

        u32 mapping_address = physical_region.physical_address;
        u32 mapping_end = mapping_address + physical_region.size;
        u32 intersection_start = std::max(mapping_address, translated_address);
        u32 intersection_end = std::min(mapping_end, translated_address + logical_size);
        if (intersection_start < intersection_end)
        {
          // Found an overlapping region; map it.

          if (is_fastmem_arena_initialized)
          {
            u32 position = physical_region.shm_position + intersection_start - mapping_address;
            u8* base = logical_base + logical_address + intersection_start - translated_address;
            u32 mapped_size = intersection_end - intersection_start;

            void* mapped_pointer = g_arena.MapInMemoryRegion(position, mapped_size, base);
            if (!mapped_pointer)
            {
              PanicAlertFmt(
                  "Memory::UpdateLogicalMemory(): Failed to map memory region at 0x{:08X} "
                  "(size 0x{:08X}) into logical fastmem region at 0x{:08X}.",
                  intersection_start, mapped_size, logical_address);
              exit(0);
            }
            logical_mapped_entries.push_back({mapped_pointer, mapped_size});
          }

          s_logical_page_mappings[i] =
              *physical_region.out_pointer + intersection_start - mapping_address;
        }
      }
    }
  }
}

void DoState(PointerWrap& p)
{
  const u32 current_ram_size = GetRamSize();
  const u32 current_l1_cache_size = GetL1CacheSize();
  const bool current_have_fake_vmem = !!m_pFakeVMEM;
  const u32 current_fake_vmem_size = current_have_fake_vmem ? GetFakeVMemSize() : 0;
  const bool current_have_exram = !!m_pEXRAM;
  const u32 current_exram_size = current_have_exram ? GetExRamSize() : 0;

  u32 state_ram_size = current_ram_size;
  u32 state_l1_cache_size = current_l1_cache_size;
  bool state_have_fake_vmem = current_have_fake_vmem;
  u32 state_fake_vmem_size = current_fake_vmem_size;
  bool state_have_exram = current_have_exram;
  u32 state_exram_size = current_exram_size;

  p.Do(state_ram_size);
  p.Do(state_l1_cache_size);
  p.Do(state_have_fake_vmem);
  p.Do(state_fake_vmem_size);
  p.Do(state_have_exram);
  p.Do(state_exram_size);

  // If we're loading a savestate and any of the above differs between the savestate and the current
  // state, cancel the load. This is technically possible to support but would require a bunch of
  // reinitialization of things that depend on these.
  if (std::tie(state_ram_size, state_l1_cache_size, state_have_fake_vmem, state_fake_vmem_size,
               state_have_exram, state_exram_size) !=
      std::tie(current_ram_size, current_l1_cache_size, current_have_fake_vmem,
               current_fake_vmem_size, current_have_exram, current_exram_size))
  {
    Core::DisplayMessage("State is incompatible with current memory settings (MMU and/or memory "
                         "overrides). Aborting load state.",
                         3000);
    p.SetVerifyMode();
    return;
  }

  p.DoArray(m_pRAM, current_ram_size);
  p.DoArray(m_pL1Cache, current_l1_cache_size);
  p.DoMarker("Memory RAM");
  if (current_have_fake_vmem)
    p.DoArray(m_pFakeVMEM, current_fake_vmem_size);
  p.DoMarker("Memory FakeVMEM");
  if (current_have_exram)
    p.DoArray(m_pEXRAM, current_exram_size);
  p.DoMarker("Memory EXRAM");
}

void Shutdown()
{
  ShutdownFastmemArena();

  m_IsInitialized = false;
  for (const PhysicalMemoryRegion& region : s_physical_regions)
  {
    if (!region.active)
      continue;

    g_arena.ReleaseView(*region.out_pointer, region.size);
    *region.out_pointer = nullptr;
  }
  g_arena.ReleaseSHMSegment();
  mmio_mapping.reset();
  INFO_LOG_FMT(MEMMAP, "Memory system shut down.");
}

void ShutdownFastmemArena()
{
  if (!is_fastmem_arena_initialized)
    return;

  for (const PhysicalMemoryRegion& region : s_physical_regions)
  {
    if (!region.active)
      continue;

    u8* base = physical_base + region.physical_address;
    g_arena.UnmapFromMemoryRegion(base, region.size);
  }

  for (auto& entry : logical_mapped_entries)
  {
    g_arena.UnmapFromMemoryRegion(entry.mapped_pointer, entry.mapped_size);
  }
  logical_mapped_entries.clear();

  g_arena.ReleaseMemoryRegion();

  physical_base = nullptr;
  logical_base = nullptr;

  is_fastmem_arena_initialized = false;
}

void Clear()
{
  if (m_pRAM)
    memset(m_pRAM, 0, GetRamSize());
  if (m_pL1Cache)
    memset(m_pL1Cache, 0, GetL1CacheSize());
  if (m_pFakeVMEM)
    memset(m_pFakeVMEM, 0, GetFakeVMemSize());
  if (m_pEXRAM)
    memset(m_pEXRAM, 0, GetExRamSize());
}

u8* GetPointerForRange(u32 address, size_t size)
{
  // Make sure we don't have a range spanning 2 separate banks
  if (size >= GetExRamSizeReal())
  {
    PanicAlertFmt("Oversized range in GetPointerForRange. {:x} bytes at {:#010x}", size, address);
    return nullptr;
  }

  // Check that the beginning and end of the range are valid
  u8* pointer = GetPointer(address);
  if (!pointer || !GetPointer(address + u32(size) - 1))
  {
    // A panic alert has already been raised by GetPointer
    return nullptr;
  }

  return pointer;
}

void CopyFromEmu(void* data, u32 address, size_t size)
{
  if (size == 0)
    return;

  void* pointer = GetPointerForRange(address, size);
  if (!pointer)
  {
    PanicAlertFmt("Invalid range in CopyFromEmu. {:x} bytes from {:#010x}", size, address);
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
    PanicAlertFmt("Invalid range in CopyToEmu. {:x} bytes to {:#010x}", size, address);
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
    PanicAlertFmt("Invalid range in Memset. {:x} bytes at {:#010x}", size, address);
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
  if (address < GetRamSizeReal())
    return m_pRAM + address;

  if (m_pEXRAM)
  {
    if ((address >> 28) == 0x1 && (address & 0x0fffffff) < GetExRamSizeReal())
      return m_pEXRAM + (address & GetExRamMask());
  }

  PanicAlertFmt("Unknown Pointer {:#010x} PC {:#010x} LR {:#010x}", address, PC, LR);
  return nullptr;
}

u8 Read_U8(u32 address)
{
  u8 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return value;
}

u16 Read_U16(u32 address)
{
  u16 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return Common::swap16(value);
}

u32 Read_U32(u32 address)
{
  u32 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return Common::swap32(value);
}

u64 Read_U64(u32 address)
{
  u64 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return Common::swap64(value);
}

void Write_U8(u8 value, u32 address)
{
  CopyToEmu(address, &value, sizeof(value));
}

void Write_U16(u16 value, u32 address)
{
  u16 swapped_value = Common::swap16(value);
  CopyToEmu(address, &swapped_value, sizeof(swapped_value));
}

void Write_U32(u32 value, u32 address)
{
  u32 swapped_value = Common::swap32(value);
  CopyToEmu(address, &swapped_value, sizeof(swapped_value));
}

void Write_U64(u64 value, u32 address)
{
  u64 swapped_value = Common::swap64(value);
  CopyToEmu(address, &swapped_value, sizeof(swapped_value));
}

void Write_U32_Swap(u32 value, u32 address)
{
  CopyToEmu(address, &value, sizeof(value));
}

void Write_U64_Swap(u64 value, u32 address)
{
  CopyToEmu(address, &value, sizeof(value));
}

}  // namespace Memory
