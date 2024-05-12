// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/MemArena.h"
#include "Common/Swap.h"
#include "Core/PowerPC/MMU.h"

// Global declarations
class PointerWrap;
namespace Core
{
class System;
}
namespace MMIO
{
class Mapping;
}

namespace Memory
{
constexpr u32 MEM1_BASE_ADDR = 0x80000000U;
constexpr u32 MEM2_BASE_ADDR = 0x90000000U;
constexpr u32 MEM1_SIZE_RETAIL = 0x01800000U;
constexpr u32 MEM1_SIZE_GDEV = 0x04000000U;
constexpr u32 MEM2_SIZE_RETAIL = 0x04000000U;
constexpr u32 MEM2_SIZE_NDEV = 0x08000000U;

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

class MemoryManager
{
public:
  explicit MemoryManager(Core::System& system);
  MemoryManager(const MemoryManager& other) = delete;
  MemoryManager(MemoryManager&& other) = delete;
  MemoryManager& operator=(const MemoryManager& other) = delete;
  MemoryManager& operator=(MemoryManager&& other) = delete;
  ~MemoryManager();

  u32 GetRamSizeReal() const { return m_ram_size_real; }
  u32 GetRamSize() const { return m_ram_size; }
  u32 GetRamMask() const { return m_ram_mask; }
  u32 GetFakeVMemSize() const { return m_fakevmem_size; }
  u32 GetFakeVMemMask() const { return m_fakevmem_mask; }
  u32 GetL1CacheSize() const { return m_l1_cache_size; }
  u32 GetL1CacheMask() const { return m_l1_cache_mask; }
  u32 GetExRamSizeReal() const { return m_exram_size_real; }
  u32 GetExRamSize() const { return m_exram_size; }
  u32 GetExRamMask() const { return m_exram_mask; }

  bool IsAddressInFastmemArea(const u8* address) const;
  u8* GetPhysicalBase() const { return m_physical_base; }
  u8* GetLogicalBase() const { return m_logical_base; }
  u8* GetPhysicalPageMappingsBase() const { return m_physical_page_mappings_base; }
  u8* GetLogicalPageMappingsBase() const { return m_logical_page_mappings_base; }

  // FIXME: these should not return their address, but AddressSpace wants that
  u8*& GetRAM() { return m_ram; }
  u8*& GetEXRAM() { return m_exram; }
  u8* GetL1Cache() { return m_l1_cache; }
  u8*& GetFakeVMEM() { return m_fake_vmem; }

  MMIO::Mapping* GetMMIOMapping() const { return m_mmio_mapping.get(); }

  // Init and Shutdown
  bool IsInitialized() const { return m_is_initialized; }
  void Init();
  void Shutdown();
  bool InitFastmemArena();
  void ShutdownFastmemArena();
  void DoState(PointerWrap& p);

  void UpdateLogicalMemory(const PowerPC::BatTable& dbat_table);

  void Clear();

  // Routines to access physically addressed memory, designed for use by
  // emulated hardware outside the CPU. Use "Device_" prefix.
  std::string GetString(u32 em_address, size_t size = 0);

  // If the specified guest address is within a valid memory region, returns a span starting at the
  // host address corresponding to the specified address and ending where the memory region ends.
  // Otherwise, returns a 0-length span starting at nullptr.
  std::span<u8> GetSpanForAddress(u32 address) const;

  // If the specified range is within a single valid memory region, returns a pointer to the start
  // of the corresponding range in host memory. Otherwise, returns nullptr.
  u8* GetPointerForRange(u32 address, size_t size) const;

  void CopyFromEmu(void* data, u32 address, size_t size) const;
  void CopyToEmu(u32 address, const void* data, size_t size);
  void Memset(u32 address, u8 value, size_t size);
  u8 Read_U8(u32 address) const;
  u16 Read_U16(u32 address) const;
  u32 Read_U32(u32 address) const;
  u64 Read_U64(u32 address) const;
  void Write_U8(u8 var, u32 address);
  void Write_U16(u16 var, u32 address);
  void Write_U32(u32 var, u32 address);
  void Write_U64(u64 var, u32 address);
  void Write_U32_Swap(u32 var, u32 address);
  void Write_U64_Swap(u64 var, u32 address);

  // Templated functions for byteswapped copies.
  template <typename T>
  void CopyFromEmuSwapped(T* data, u32 address, size_t size) const
  {
    const T* src = reinterpret_cast<T*>(GetPointerForRange(address, size));

    if (src == nullptr)
      return;

    for (size_t i = 0; i < size / sizeof(T); i++)
      data[i] = Common::FromBigEndian(src[i]);
  }

  template <typename T>
  void CopyToEmuSwapped(u32 address, const T* data, size_t size)
  {
    T* dest = reinterpret_cast<T*>(GetPointerForRange(address, size));

    if (dest == nullptr)
      return;

    for (size_t i = 0; i < size / sizeof(T); i++)
      dest[i] = Common::FromBigEndian(data[i]);
  }

private:
  // Base is a pointer to the base of the memory map. Yes, some MMU tricks
  // are used to set up a full GC or Wii memory map in process memory.
  // In 64-bit, this might point to "high memory" (above the 32-bit limit),
  // so be sure to load it into a 64-bit register.
  u8* m_fastmem_arena = nullptr;
  size_t m_fastmem_arena_size = 0;
  u8* m_physical_base = nullptr;
  u8* m_logical_base = nullptr;

  // This page table is used for a "soft MMU" implementation when
  // setting up the full memory map in process memory isn't possible.
  u8* m_physical_page_mappings_base = nullptr;
  u8* m_logical_page_mappings_base = nullptr;

  // The actual memory used for backing the memory map.
  u8* m_ram = nullptr;
  u8* m_exram = nullptr;
  u8* m_l1_cache = nullptr;
  u8* m_fake_vmem = nullptr;

  // m_ram_size is the amount allocated by the emulator, whereas m_ram_size_real
  // is what will be reported in lowmem, and thus used by emulated software.
  // Note: Writing to lowmem is done by IPL. If using retail IPL, it will
  // always be set to 24MB.
  u32 m_ram_size_real = 0;
  u32 m_ram_size = 0;
  u32 m_ram_mask = 0;
  u32 m_fakevmem_size = 0;
  u32 m_fakevmem_mask = 0;
  u32 m_l1_cache_size = 0;
  u32 m_l1_cache_mask = 0;
  // m_exram_size is the amount allocated by the emulator, whereas m_exram_size_real
  // is what gets used by emulated software.  If using retail IOS, it will
  // always be set to 64MB.
  u32 m_exram_size_real = 0;
  u32 m_exram_size = 0;
  u32 m_exram_mask = 0;

  bool m_is_fastmem_arena_initialized = false;

  // STATE_TO_SAVE
  // Save the Init(), Shutdown() state
  bool m_is_initialized = false;
  // END STATE_TO_SAVE

  // MMIO mapping object.
  std::unique_ptr<MMIO::Mapping> m_mmio_mapping;

  // The MemArena class
  Common::MemArena m_arena;

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
  // The 4GB starting at m_physical_base represents access from the CPU
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
  // The 4GB starting at m_logical_base represents access from the CPU
  // with address translation turned on.  This mapping is computed based
  // on the BAT registers.
  //
  // Each of these 4GB regions is surrounded by 2GB of empty space so overflows
  // in address computation in the JIT don't access unrelated memory.
  //
  // The neighboring mirrors of RAM ([0x02000000, 0x08000000), etc.) exist because
  // the bus masks off the bits in question for RAM accesses; using them is a
  // terrible idea because the CPU cache won't handle them correctly, but a
  // few buggy games (notably Rogue Squadron 2) use them by accident. They
  // aren't backed by memory mappings because they are used very rarely.
  //
  // TODO: The actual size of RAM is 24MB; the other 8MB shouldn't be backed by actual memory.
  // TODO: Do we want to handle the mirrors of the GC RAM?
  std::array<PhysicalMemoryRegion, 4> m_physical_regions{};

  std::vector<LogicalMemoryView> m_logical_mapped_entries;

  std::array<void*, PowerPC::BAT_PAGE_COUNT> m_physical_page_mappings{};
  std::array<void*, PowerPC::BAT_PAGE_COUNT> m_logical_page_mappings{};

  Core::System& m_system;

  void InitMMIO(bool is_wii);
};
}  // namespace Memory
