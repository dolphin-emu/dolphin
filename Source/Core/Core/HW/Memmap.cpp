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
MemoryManager::MemoryManager() = default;
MemoryManager::~MemoryManager() = default;

void MemoryManager::InitMMIO(bool is_wii)
{
  m_mmio_mapping = std::make_unique<MMIO::Mapping>();

  auto& system = Core::System::GetInstance();
  system.GetCommandProcessor().RegisterMMIO(system, m_mmio_mapping.get(), 0x0C000000);
  PixelEngine::RegisterMMIO(m_mmio_mapping.get(), 0x0C001000);
  VideoInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C002000);
  ProcessorInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C003000);
  MemoryInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C004000);
  DSP::RegisterMMIO(m_mmio_mapping.get(), 0x0C005000);
  DVDInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C006000, false);
  SerialInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C006400);
  ExpansionInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C006800);
  AudioInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0C006C00);
  if (is_wii)
  {
    IOS::RegisterMMIO(m_mmio_mapping.get(), 0x0D000000);
    DVDInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0D006000, true);
    SerialInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0D006400);
    ExpansionInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0D006800);
    AudioInterface::RegisterMMIO(m_mmio_mapping.get(), 0x0D006C00);
  }
}

void MemoryManager::Init()
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
  m_ram_size_real = get_mem1_size();
  m_ram_size = MathUtil::NextPowerOf2(GetRamSizeReal());
  m_ram_mask = GetRamSize() - 1;
  m_fakevmem_size = 0x02000000;
  m_fakevmem_mask = GetFakeVMemSize() - 1;
  m_l1_cache_size = 0x00040000;
  m_l1_cache_mask = GetL1CacheSize() - 1;
  m_exram_size_real = get_mem2_size();
  m_exram_size = MathUtil::NextPowerOf2(GetExRamSizeReal());
  m_exram_mask = GetExRamSize() - 1;

  m_physical_regions[0] = PhysicalMemoryRegion{
      &m_ram, 0x00000000, GetRamSize(), PhysicalMemoryRegion::ALWAYS, 0, false};
  m_physical_regions[1] = PhysicalMemoryRegion{
      &m_l1_cache, 0xE0000000, GetL1CacheSize(), PhysicalMemoryRegion::ALWAYS, 0, false};
  m_physical_regions[2] = PhysicalMemoryRegion{
      &m_fake_vmem, 0x7E000000, GetFakeVMemSize(), PhysicalMemoryRegion::FAKE_VMEM, 0, false};
  m_physical_regions[3] = PhysicalMemoryRegion{
      &m_exram, 0x10000000, GetExRamSize(), PhysicalMemoryRegion::WII_ONLY, 0, false};

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
  for (PhysicalMemoryRegion& region : m_physical_regions)
  {
    if (!wii && (region.flags & PhysicalMemoryRegion::WII_ONLY))
      continue;
    if (!fake_vmem && (region.flags & PhysicalMemoryRegion::FAKE_VMEM))
      continue;

    region.shm_position = mem_size;
    region.active = true;
    mem_size += region.size;
  }
  m_arena.GrabSHMSegment(mem_size);

  m_physical_page_mappings.fill(nullptr);

  // Create an anonymous view of the physical memory
  for (const PhysicalMemoryRegion& region : m_physical_regions)
  {
    if (!region.active)
      continue;

    *region.out_pointer = (u8*)m_arena.CreateView(region.shm_position, region.size);

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
      m_physical_page_mappings[index] = *region.out_pointer + i;
    }
  }

  m_physical_page_mappings_base = reinterpret_cast<u8*>(m_physical_page_mappings.data());
  m_logical_page_mappings_base = reinterpret_cast<u8*>(m_logical_page_mappings.data());

  InitMMIO(wii);

  Clear();

  INFO_LOG_FMT(MEMMAP, "Memory system initialized. RAM at {}", fmt::ptr(m_ram));
  m_is_initialized = true;
}

bool MemoryManager::InitFastmemArena()
{
#if _ARCH_32
  const size_t memory_size = 0x31000000;
#else
  const size_t memory_size = 0x400000000;
#endif
  m_physical_base = m_arena.ReserveMemoryRegion(memory_size);

  if (!m_physical_base)
  {
    PanicAlertFmt("Memory::InitFastmemArena(): Failed finding a memory base.");
    return false;
  }

  for (const PhysicalMemoryRegion& region : m_physical_regions)
  {
    if (!region.active)
      continue;

    u8* base = m_physical_base + region.physical_address;
    u8* view = (u8*)m_arena.MapInMemoryRegion(region.shm_position, region.size, base);

    if (base != view)
    {
      PanicAlertFmt("Memory::InitFastmemArena(): Failed to map memory region at 0x{:08X} "
                    "(size 0x{:08X}) into physical fastmem region.",
                    region.physical_address, region.size);
      return false;
    }
  }

#ifndef _ARCH_32
  m_logical_base = m_physical_base + 0x200000000;
#endif

  m_is_fastmem_arena_initialized = true;
  return true;
}

void MemoryManager::UpdateLogicalMemory(const PowerPC::BatTable& dbat_table)
{
  for (auto& entry : m_logical_mapped_entries)
  {
    m_arena.UnmapFromMemoryRegion(entry.mapped_pointer, entry.mapped_size);
  }
  m_logical_mapped_entries.clear();

  m_logical_page_mappings.fill(nullptr);

  for (u32 i = 0; i < dbat_table.size(); ++i)
  {
    if (dbat_table[i] & PowerPC::BAT_PHYSICAL_BIT)
    {
      u32 logical_address = i << PowerPC::BAT_INDEX_SHIFT;
      // TODO: Merge adjacent mappings to make this faster.
      u32 logical_size = PowerPC::BAT_PAGE_SIZE;
      u32 translated_address = dbat_table[i] & PowerPC::BAT_RESULT_MASK;
      for (const auto& physical_region : m_physical_regions)
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

          if (m_is_fastmem_arena_initialized)
          {
            u32 position = physical_region.shm_position + intersection_start - mapping_address;
            u8* base = m_logical_base + logical_address + intersection_start - translated_address;
            u32 mapped_size = intersection_end - intersection_start;

            void* mapped_pointer = m_arena.MapInMemoryRegion(position, mapped_size, base);
            if (!mapped_pointer)
            {
              PanicAlertFmt(
                  "Memory::UpdateLogicalMemory(): Failed to map memory region at 0x{:08X} "
                  "(size 0x{:08X}) into logical fastmem region at 0x{:08X}.",
                  intersection_start, mapped_size, logical_address);
              exit(0);
            }
            m_logical_mapped_entries.push_back({mapped_pointer, mapped_size});
          }

          m_logical_page_mappings[i] =
              *physical_region.out_pointer + intersection_start - mapping_address;
        }
      }
    }
  }
}

void MemoryManager::DoState(PointerWrap& p)
{
  const u32 current_ram_size = GetRamSize();
  const u32 current_l1_cache_size = GetL1CacheSize();
  const bool current_have_fake_vmem = !!m_fake_vmem;
  const u32 current_fake_vmem_size = current_have_fake_vmem ? GetFakeVMemSize() : 0;
  const bool current_have_exram = !!m_exram;
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

  p.DoArray(m_ram, current_ram_size);
  p.DoArray(m_l1_cache, current_l1_cache_size);
  p.DoMarker("Memory RAM");
  if (current_have_fake_vmem)
    p.DoArray(m_fake_vmem, current_fake_vmem_size);
  p.DoMarker("Memory FakeVMEM");
  if (current_have_exram)
    p.DoArray(m_exram, current_exram_size);
  p.DoMarker("Memory EXRAM");
}

void MemoryManager::Shutdown()
{
  ShutdownFastmemArena();

  m_is_initialized = false;
  for (const PhysicalMemoryRegion& region : m_physical_regions)
  {
    if (!region.active)
      continue;

    m_arena.ReleaseView(*region.out_pointer, region.size);
    *region.out_pointer = nullptr;
  }
  m_arena.ReleaseSHMSegment();
  m_mmio_mapping.reset();
  INFO_LOG_FMT(MEMMAP, "Memory system shut down.");
}

void MemoryManager::ShutdownFastmemArena()
{
  if (!m_is_fastmem_arena_initialized)
    return;

  for (const PhysicalMemoryRegion& region : m_physical_regions)
  {
    if (!region.active)
      continue;

    u8* base = m_physical_base + region.physical_address;
    m_arena.UnmapFromMemoryRegion(base, region.size);
  }

  for (auto& entry : m_logical_mapped_entries)
  {
    m_arena.UnmapFromMemoryRegion(entry.mapped_pointer, entry.mapped_size);
  }
  m_logical_mapped_entries.clear();

  m_arena.ReleaseMemoryRegion();

  m_physical_base = nullptr;
  m_logical_base = nullptr;

  m_is_fastmem_arena_initialized = false;
}

void MemoryManager::Clear()
{
  if (m_ram)
    memset(m_ram, 0, GetRamSize());
  if (m_l1_cache)
    memset(m_l1_cache, 0, GetL1CacheSize());
  if (m_fake_vmem)
    memset(m_fake_vmem, 0, GetFakeVMemSize());
  if (m_exram)
    memset(m_exram, 0, GetExRamSize());
}

u8* MemoryManager::GetPointerForRange(u32 address, size_t size) const
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

void MemoryManager::CopyFromEmu(void* data, u32 address, size_t size) const
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

void MemoryManager::CopyToEmu(u32 address, const void* data, size_t size)
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

void MemoryManager::Memset(u32 address, u8 value, size_t size)
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

std::string MemoryManager::GetString(u32 em_address, size_t size)
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

u8* MemoryManager::GetPointer(u32 address) const
{
  // TODO: Should we be masking off more bits here?  Can all devices access
  // EXRAM?
  address &= 0x3FFFFFFF;
  if (address < GetRamSizeReal())
    return m_ram + address;

  if (m_exram)
  {
    if ((address >> 28) == 0x1 && (address & 0x0fffffff) < GetExRamSizeReal())
      return m_exram + (address & GetExRamMask());
  }

  PanicAlertFmt("Unknown Pointer {:#010x} PC {:#010x} LR {:#010x}", address, PC, LR);
  return nullptr;
}

u8 MemoryManager::Read_U8(u32 address) const
{
  u8 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return value;
}

u16 MemoryManager::Read_U16(u32 address) const
{
  u16 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return Common::swap16(value);
}

u32 MemoryManager::Read_U32(u32 address) const
{
  u32 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return Common::swap32(value);
}

u64 MemoryManager::Read_U64(u32 address) const
{
  u64 value = 0;
  CopyFromEmu(&value, address, sizeof(value));
  return Common::swap64(value);
}

void MemoryManager::Write_U8(u8 value, u32 address)
{
  CopyToEmu(address, &value, sizeof(value));
}

void MemoryManager::Write_U16(u16 value, u32 address)
{
  u16 swapped_value = Common::swap16(value);
  CopyToEmu(address, &swapped_value, sizeof(swapped_value));
}

void MemoryManager::Write_U32(u32 value, u32 address)
{
  u32 swapped_value = Common::swap32(value);
  CopyToEmu(address, &swapped_value, sizeof(swapped_value));
}

void MemoryManager::Write_U64(u64 value, u32 address)
{
  u64 swapped_value = Common::swap64(value);
  CopyToEmu(address, &swapped_value, sizeof(swapped_value));
}

void MemoryManager::Write_U32_Swap(u32 value, u32 address)
{
  CopyToEmu(address, &value, sizeof(value));
}

void MemoryManager::Write_U64_Swap(u64 value, u32 address)
{
  CopyToEmu(address, &value, sizeof(value));
}

}  // namespace Memory
