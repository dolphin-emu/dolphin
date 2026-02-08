// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/PPCCache.h"

#include <algorithm>
#include <array>

#include "Common/ChunkFile.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace PowerPC
{
namespace
{
constexpr std::array<u32, 8> s_plru_mask{
    11, 11, 19, 19, 37, 37, 69, 69,
};
constexpr std::array<u32, 8> s_plru_value{
    11, 3, 17, 1, 36, 4, 64, 0,
};

constexpr std::array<u32, 255> s_way_from_valid = [] {
  std::array<u32, 255> data{};
  for (size_t m = 0; m < data.size(); m++)
  {
    u32 w = 0;
    while ((m & (size_t{1} << w)) != 0)
      w++;
    data[m] = w;
  }
  return data;
}();

constexpr std::array<u32, 128> s_way_from_plru = [] {
  std::array<u32, 128> data{};

  for (size_t m = 0; m < data.size(); m++)
  {
    std::array<u32, 7> b{};
    for (size_t i = 0; i < b.size(); i++)
      b[i] = u32(m & (size_t{1} << i));

    u32 w = 0;
    if (b[0] != 0)
    {
      if (b[2] != 0)
      {
        if (b[6] != 0)
          w = 7;
        else
          w = 6;
      }
      else if (b[5] != 0)
      {
        w = 5;
      }
      else
      {
        w = 4;
      }
    }
    else if (b[1] != 0)
    {
      if (b[4] != 0)
        w = 3;
      else
        w = 2;
    }
    else if (b[3] != 0)
    {
      w = 1;
    }
    else
    {
      w = 0;
    }

    data[m] = w;
  }

  return data;
}();
}  // Anonymous namespace

InstructionCache::~InstructionCache()
{
  if (m_config_callback_id)
    Config::RemoveConfigChangedCallback(*m_config_callback_id);
}

void Cache::Reset()
{
  valid.fill(0);
  plru.fill(0);
  modified.fill(0);
  std::ranges::fill(lookup_table, 0xFF);
  std::ranges::fill(lookup_table_ex, 0xFF);
  std::ranges::fill(lookup_table_vmem, 0xFF);
}

void InstructionCache::Reset(JitInterface& jit_interface)
{
  Cache::Reset();
  jit_interface.ClearSafe();
}

void Cache::Init(Memory::MemoryManager& memory)
{
  data.fill({});
  addrs.fill({});
  lookup_table.resize(memory.GetRamSize() >> 5);
  lookup_table_ex.resize(memory.GetExRamSize() >> 5);
  lookup_table_vmem.resize(memory.GetFakeVMemSize() >> 5);
  Reset();
}

void InstructionCache::Init(Memory::MemoryManager& memory)
{
  if (!m_config_callback_id)
    m_config_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();

  Cache::Init(memory);
}

void Cache::Store(Memory::MemoryManager& memory, u32 addr)
{
  auto [set, way] = GetCache(memory, addr, true);

  if (way == 0xff)
    return;

  if (valid[set] & (1U << way) && modified[set] & (1U << way))
    memory.CopyToEmu((addr & ~0x1f), data[set][way].data(), 32);
  modified[set] &= ~(1U << way);
}

void Cache::FlushAll(Memory::MemoryManager& memory)
{
  for (size_t set = 0; set < CACHE_SETS; set++)
  {
    for (size_t way = 0; way < CACHE_WAYS; way++)
    {
      if (valid[set] & (1U << way) && modified[set] & (1U << way))
        memory.CopyToEmu(addrs[set][way], data[set][way].data(), 32);
    }
  }

  Reset();
}

void Cache::Invalidate(Memory::MemoryManager& memory, u32 addr)
{
  auto [set, way] = GetCache(memory, addr, true);

  if (way == 0xff)
    return;

  if (valid[set] & (1U << way))
  {
    if (addrs[set][way] & CACHE_VMEM_BIT)
      lookup_table_vmem[(addrs[set][way] & memory.GetFakeVMemMask()) >> 5] = 0xff;
    else if (addrs[set][way] & CACHE_EXRAM_BIT)
      lookup_table_ex[(addrs[set][way] & memory.GetExRamMask()) >> 5] = 0xff;
    else
      lookup_table[(addrs[set][way] & memory.GetRamMask()) >> 5] = 0xff;

    valid[set] &= ~(1U << way);
    modified[set] &= ~(1U << way);
  }
}

void Cache::Flush(Memory::MemoryManager& memory, u32 addr)
{
  auto [set, way] = GetCache(memory, addr, true);

  if (way == 0xff)
    return;

  if (valid[set] & (1U << way))
  {
    if (modified[set] & (1U << way))
      memory.CopyToEmu((addr & ~0x1f), data[set][way].data(), 32);

    if (addrs[set][way] & CACHE_VMEM_BIT)
      lookup_table_vmem[(addrs[set][way] & memory.GetFakeVMemMask()) >> 5] = 0xff;
    else if (addrs[set][way] & CACHE_EXRAM_BIT)
      lookup_table_ex[(addrs[set][way] & memory.GetExRamMask()) >> 5] = 0xff;
    else
      lookup_table[(addrs[set][way] & memory.GetRamMask()) >> 5] = 0xff;

    valid[set] &= ~(1U << way);
    modified[set] &= ~(1U << way);
  }
}

void Cache::Touch(Memory::MemoryManager& memory, u32 addr, bool store)
{
  GetCache(memory, addr, false);
}

std::pair<u32, u32> Cache::GetCache(Memory::MemoryManager& memory, u32 addr, bool locked)
{
  addr &= ~31;
  u32 set = (addr >> 5) & 0x7f;
  u32 way;

  if (addr & CACHE_VMEM_BIT)
  {
    way = lookup_table_vmem[(addr & memory.GetFakeVMemMask()) >> 5];
  }
  else if (addr & CACHE_EXRAM_BIT)
  {
    way = lookup_table_ex[(addr & memory.GetExRamMask()) >> 5];
  }
  else
  {
    way = lookup_table[(addr & memory.GetRamMask()) >> 5];
  }

  // load to the cache
  if (!locked && way == 0xff)
  {
    // select a way
    if (valid[set] != 0xff)
      way = s_way_from_valid[valid[set]];
    else
      way = s_way_from_plru[plru[set]];

    if (valid[set] & (1 << way))
    {
      // store the cache back to main memory
      if (modified[set] & (1 << way))
        memory.CopyToEmu(addrs[set][way], data[set][way].data(), 32);

      if (addrs[set][way] & CACHE_VMEM_BIT)
        lookup_table_vmem[(addrs[set][way] & memory.GetFakeVMemMask()) >> 5] = 0xff;
      else if (addrs[set][way] & CACHE_EXRAM_BIT)
        lookup_table_ex[(addrs[set][way] & memory.GetExRamMask()) >> 5] = 0xff;
      else
        lookup_table[(addrs[set][way] & memory.GetRamMask()) >> 5] = 0xff;
    }

    // load
    memory.CopyFromEmu(data[set][way].data(), (addr & ~0x1f), 32);

    if (addr & CACHE_VMEM_BIT)
      lookup_table_vmem[(addr & memory.GetFakeVMemMask()) >> 5] = way;
    else if (addr & CACHE_EXRAM_BIT)
      lookup_table_ex[(addr & memory.GetExRamMask()) >> 5] = way;
    else
      lookup_table[(addr & memory.GetRamMask()) >> 5] = way;

    addrs[set][way] = addr;
    valid[set] |= (1 << way);
    modified[set] &= ~(1 << way);
  }

  // update plru
  if (way != 0xff)
    plru[set] = (plru[set] & ~s_plru_mask[way]) | s_plru_value[way];

  return {set, way};
}

void Cache::Read(Memory::MemoryManager& memory, u32 addr, void* buffer, u32 len, bool locked)
{
  auto* value = static_cast<u8*>(buffer);

  while (len > 0)
  {
    auto [set, way] = GetCache(memory, addr, locked);

    u32 offset_in_block = addr - (addr & ~31);
    u32 len_in_block = std::min<u32>(len, ((addr + 32) & ~31) - addr);

    if (way != 0xff)
    {
      std::memcpy(value, reinterpret_cast<u8*>(data[set][way].data()) + offset_in_block,
                  len_in_block);
    }
    else
    {
      memory.CopyFromEmu(value, addr, len_in_block);
    }

    addr += len_in_block;
    len -= len_in_block;
    value += len_in_block;
  }
}

void Cache::Write(Memory::MemoryManager& memory, u32 addr, const void* buffer, u32 len, bool locked)
{
  auto* value = static_cast<const u8*>(buffer);

  while (len > 0)
  {
    auto [set, way] = GetCache(memory, addr, locked);

    u32 offset_in_block = addr - (addr & ~31);
    u32 len_in_block = std::min<u32>(len, ((addr + 32) & ~31) - addr);

    if (way != 0xff)
    {
      std::memcpy(reinterpret_cast<u8*>(data[set][way].data()) + offset_in_block, value,
                  len_in_block);
      modified[set] |= (1 << way);
    }
    else
    {
      memory.CopyToEmu(addr, value, len_in_block);
    }

    addr += len_in_block;
    len -= len_in_block;
    value += len_in_block;
  }
}

void Cache::DoState(Memory::MemoryManager& memory, PointerWrap& p)
{
  if (p.IsReadMode())
  {
    // Clear valid parts of the lookup tables (this is done instead of using fill(0xff) to avoid
    // loading the entire 4MB of tables into cache)
    for (u32 set = 0; set < CACHE_SETS; set++)
    {
      for (u32 way = 0; way < CACHE_WAYS; way++)
      {
        if ((valid[set] & (1 << way)) != 0)
        {
          if (addrs[set][way] & CACHE_VMEM_BIT)
            lookup_table_vmem[(addrs[set][way] & memory.GetFakeVMemMask()) >> 5] = 0xff;
          else if (addrs[set][way] & CACHE_EXRAM_BIT)
            lookup_table_ex[(addrs[set][way] & memory.GetExRamMask()) >> 5] = 0xff;
          else
            lookup_table[(addrs[set][way] & memory.GetRamMask()) >> 5] = 0xff;
        }
      }
    }
  }

  p.DoArray(data);
  p.DoArray(plru);
  p.DoArray(valid);
  p.DoArray(addrs);
  p.DoArray(modified);

  if (p.IsReadMode())
  {
    // Recompute lookup tables
    for (u32 set = 0; set < CACHE_SETS; set++)
    {
      for (u32 way = 0; way < CACHE_WAYS; way++)
      {
        if ((valid[set] & (1 << way)) != 0)
        {
          if (addrs[set][way] & CACHE_VMEM_BIT)
            lookup_table_vmem[(addrs[set][way] & memory.GetFakeVMemMask()) >> 5] = way;
          else if (addrs[set][way] & CACHE_EXRAM_BIT)
            lookup_table_ex[(addrs[set][way] & memory.GetExRamMask()) >> 5] = way;
          else
            lookup_table[(addrs[set][way] & memory.GetRamMask()) >> 5] = way;
        }
      }
    }
  }
}

u32 InstructionCache::ReadInstruction(Memory::MemoryManager& memory,
                                      PowerPC::PowerPCState& ppc_state, u32 addr)
{
  if (!HID0(ppc_state).ICE || m_disable_icache)  // instruction cache is disabled
    return memory.Read_U32(addr);

  u32 value;
  Read(memory, addr, &value, sizeof(value), HID0(ppc_state).ILOCK);
  return Common::swap32(value);
}

void InstructionCache::Invalidate(Memory::MemoryManager& memory, JitInterface& jit_interface,
                                  u32 addr)
{
  // Per the 750cl manual, section 3.4.1.5 Instruction Cache Enabling/Disabling (page 137)
  // and section 3.4.2.6 Instruction Cache Block Invalidate (icbi) (page 140), the icbi
  // instruction always invalidates, even if the instruction cache is disabled or locked,
  // and it also invalidates all ways of the corresponding cache set, not just the way corresponding
  // to the given address.
  // (However, the icbi instruction's info on page 432 does not include this information)
  const u32 set = (addr >> 5) & 0x7f;
  for (size_t way = 0; way < 8; way++)
  {
    if (valid[set] & (1U << way))
    {
      if (addrs[set][way] & CACHE_VMEM_BIT)
        lookup_table_vmem[(addrs[set][way] & memory.GetFakeVMemMask()) >> 5] = 0xff;
      else if (addrs[set][way] & CACHE_EXRAM_BIT)
        lookup_table_ex[(addrs[set][way] & memory.GetExRamMask()) >> 5] = 0xff;
      else
        lookup_table[(addrs[set][way] & memory.GetRamMask()) >> 5] = 0xff;
    }
  }
  valid[set] = 0;
  modified[set] = 0;

  // Also tell the JIT that the corresponding address has been invalidated
  jit_interface.InvalidateICacheLine(addr);
}

void InstructionCache::RefreshConfig()
{
  m_disable_icache = Config::Get(Config::MAIN_DISABLE_ICACHE);
}
}  // namespace PowerPC
