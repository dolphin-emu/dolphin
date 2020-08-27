// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PPCCache.h"

#include <array>

#include "Common/ChunkFile.h"
#include "Common/Swap.h"
#include "Core/Analytics.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

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

InstructionCache::InstructionCache()
{
}

void InstructionCache::Reset()
{
  valid.fill(0);
  plru.fill(0);
  lookup_table.fill(0xFF);
  lookup_table_ex.fill(0xFF);
  lookup_table_vmem.fill(0xFF);
  JitInterface::ClearSafe();
}

void InstructionCache::Init()
{
  data.fill({});
  tags.fill({});
  Reset();
}

void InstructionCache::Invalidate(u32 addr)
{
  if (!HID0.ICE)
    return;

  // Invalidates the whole set
  const u32 set = (addr >> 5) & 0x7f;
  for (size_t i = 0; i < 8; i++)
  {
    if (valid[set] & (1U << i))
    {
      if (tags[set][i] & (ICACHE_VMEM_BIT >> 12))
        lookup_table_vmem[((tags[set][i] << 7) | set) & 0xfffff] = 0xff;
      else if (tags[set][i] & (ICACHE_EXRAM_BIT >> 12))
        lookup_table_ex[((tags[set][i] << 7) | set) & 0x1fffff] = 0xff;
      else
        lookup_table[((tags[set][i] << 7) | set) & 0xfffff] = 0xff;
    }
  }
  valid[set] = 0;
  JitInterface::InvalidateICache(addr & ~0x1f, 32, false);
}

u32 InstructionCache::ReadInstruction(u32 addr)
{
  if (!HID0.ICE)  // instruction cache is disabled
    return Memory::Read_U32(addr);
  u32 set = (addr >> 5) & 0x7f;
  u32 tag = addr >> 12;

  u32 t;
  if (addr & ICACHE_VMEM_BIT)
  {
    t = lookup_table_vmem[(addr >> 5) & 0xfffff];
  }
  else if (addr & ICACHE_EXRAM_BIT)
  {
    t = lookup_table_ex[(addr >> 5) & 0x1fffff];
  }
  else
  {
    t = lookup_table[(addr >> 5) & 0xfffff];
  }

  if (t == 0xff)  // load to the cache
  {
    if (HID0.ILOCK)  // instruction cache is locked
      return Memory::Read_U32(addr);
    // select a way
    if (valid[set] != 0xff)
      t = s_way_from_valid[valid[set]];
    else
      t = s_way_from_plru[plru[set]];
    // load
    Memory::CopyFromEmu(reinterpret_cast<u8*>(data[set][t].data()), (addr & ~0x1f), 32);
    if (valid[set] & (1 << t))
    {
      if (tags[set][t] & (ICACHE_VMEM_BIT >> 12))
        lookup_table_vmem[((tags[set][t] << 7) | set) & 0xfffff] = 0xff;
      else if (tags[set][t] & (ICACHE_EXRAM_BIT >> 12))
        lookup_table_ex[((tags[set][t] << 7) | set) & 0x1fffff] = 0xff;
      else
        lookup_table[((tags[set][t] << 7) | set) & 0xfffff] = 0xff;
    }

    if (addr & ICACHE_VMEM_BIT)
      lookup_table_vmem[(addr >> 5) & 0xfffff] = t;
    else if (addr & ICACHE_EXRAM_BIT)
      lookup_table_ex[(addr >> 5) & 0x1fffff] = t;
    else
      lookup_table[(addr >> 5) & 0xfffff] = t;
    tags[set][t] = tag;
    valid[set] |= (1 << t);
  }
  // update plru
  plru[set] = (plru[set] & ~s_plru_mask[t]) | s_plru_value[t];
  u32 res = Common::swap32(data[set][t][(addr >> 2) & 7]);
  u32 inmem = Memory::Read_U32(addr);
  if (res != inmem)
  {
    INFO_LOG(POWERPC, "ICache read at %08x returned stale data: CACHED: %08x vs. RAM: %08x", addr,
             res, inmem);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::ICACHE_MATTERS);
  }
  return res;
}

void InstructionCache::DoState(PointerWrap& p)
{
  p.DoArray(data);
  p.DoArray(tags);
  p.DoArray(plru);
  p.DoArray(valid);
  p.DoArray(lookup_table);
  p.DoArray(lookup_table_ex);
  p.DoArray(lookup_table_vmem);
}
}  // namespace PowerPC
