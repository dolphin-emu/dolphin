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
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemArena.h"
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
static Common::MemArena g_arena;
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
  physical_base = Common::MemArena::FindMemoryBase();

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
          u32 position = physical_region.shm_position + intersection_start - mapping_address;
          u8* base = logical_base + logical_address + intersection_start - translated_address;
          u32 mapped_size = intersection_end - intersection_start;

          void* mapped_pointer = g_arena.CreateView(position, mapped_size, base);
          if (!mapped_pointer)
          {
            PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
            exit(0);
          }
          logical_mapped_entries.push_back({mapped_pointer, mapped_size});
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

}  // namespace Memory
