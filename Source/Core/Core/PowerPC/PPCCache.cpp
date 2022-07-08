// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/PPCCache.h"

#include <array>

#include "Common/ChunkFile.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef _MSC_VER
// C++20 is used only for better compile-time checks (constexpr std::all_of and also consteval)
#define ICACHE_HAS_CXX_20
#define ICACHE_CONSTEVAL consteval
#else
#define ICACHE_CONSTEVAL constexpr
#endif

namespace PowerPC
{
// This is a tree-based Pseudo Least Recently Used (PLRU) algorithm, and matches the algorithm
// described in the ppc_750cl manual for the instruction cache (and the data cache, although the
// data cache uses 3 states (invalid (I), exclusive (E), and modified (M)) while the instruction
// cache only has two (invalid (I) and valid (V)).
namespace
{
constexpr u32 NUM_PLRU_BITS = ICACHE_WAYS - 1;
constexpr u8 NUM_PLRU_BIT_VALUES = 1 << NUM_PLRU_BITS;
constexpr u8 PLRU_BIT_MASK = NUM_PLRU_BIT_VALUES - 1;

// See Table 3-3. PLRU Bit Update Rules on page 143 of the 750cl manual.
// The first entry in the pair is the mask indicating which bits to update.
// The second entry is the value to set those bits to. This value correspons with making all parent
// nodes in a tree point away from the entry that was most recently updated; for instance, L2 is
// reached by B0 = 0, then B1 = 1, then B4 = 1, so when L2 is used, the algorithm sets B0 to 1,
// B1 to 0, and B4 to 0, and leaves the other bits unchanged.
//
// To ensure alignment with the comment, an 8-bit binary literal is used below, but there are
// actually only 7 PLRU bits. Additionally, note that the table shows B0 as the leftmost bit and B6
// as the rightmost bit; the order is reversed here.
constexpr std::array<std::pair<u8, u8>, ICACHE_WAYS> PLRU_RULES{{
    {
        // ___1_11 for access to L0
        0b00001011,
        0b00001011,
    },
    {
        // ___0_11 for access to L1
        0b00001011,
        0b00000011,
    },
    {
        // __1__01 for access to L2
        0b00010011,
        0b00010001,
    },
    {
        // __0__01 for access to L3
        0b00010011,
        0b00000001,
    },
    {
        // _1__1_0 for access to L4
        0b00100101,
        0b00100100,
    },
    {
        // _0__1_0 for access to L5
        0b00100101,
        0b00000100,
    },
    {
        // 1___0_0 for access to L6
        0b01000101,
        0b01000000,
    },
    {
        // 0___0_0 for access to L7
        0b01000101,
        0b00000000,
    },
}};
#ifdef ICACHE_HAS_CXX_20
static_assert(std::all_of(PLRU_RULES.begin(), PLRU_RULES.end(),
                          [](auto& pair) { return (pair.first & ~PLRU_BIT_MASK) == 0; }),
              "PLRU mask contains bits not matching the number of PLRU bits");
static_assert(std::all_of(PLRU_RULES.begin(), PLRU_RULES.end(),
                          [](auto& pair) { return (pair.second & ~pair.first) == 0; }),
              "PLRU new value should not contain any bits not set in the corresponding mask");
#endif

// See the top half of Figure 3-5. PLRU Replacement Algorithm on page 142 of the 750cl manual.
// This is the lowest-order invalid block in the set.
ICACHE_CONSTEVAL u8 WayFromValid(u8 valid_bits)
{
  u8 lowest_invalid_block = 0;
  while ((valid_bits & (1 << lowest_invalid_block)) != 0)
  {
    lowest_invalid_block++;
  }
  return lowest_invalid_block;
}
// This array is a used to pre-compute which way to replace if an invalid way exists.
// Note that this array has a size of 255, not 256; if all ways are valid,
// then WAY_FROM_PLRU must be used instead.
constexpr std::array<u8, (1 << ICACHE_WAYS) - 1> WAY_FROM_VALID = []() ICACHE_CONSTEVAL {
  std::array<u8, (1 << ICACHE_WAYS) - 1> result{};
  for (u8 valid_bits = 0; valid_bits < result.size(); valid_bits++)
    result[valid_bits] = WayFromValid(valid_bits);
  return result;
}();
#ifdef ICACHE_HAS_CXX_20
static_assert(std::all_of(WAY_FROM_VALID.begin(), WAY_FROM_VALID.end(),
                          [](u8 way) { return way < ICACHE_WAYS; }),
              "WAY_FROM_VALID must reference valid ways only");
#endif

// See the bottom half of Figure 3-5. PLRU Replacement Algorithm on page 142 of the 750cl manual,
// or Table 3-4. PLRU Replacement Block Selection on page 143.
// PLRU bits are numbered in a breadth-first manner.
ICACHE_CONSTEVAL u8 WayFromPLRU(u8 plru_bits)
{
  if ((plru_bits & (1 << 0)) == 0)  // B0 = 0
  {
    if ((plru_bits & (1 << 1)) == 0)  // B1 = 0
    {
      if ((plru_bits & (1 << 3)) == 0)  // B3 = 0
      {
        return 0;
      }
      else  // B3 = 1
      {
        return 1;
      }
    }
    else  // B1 = 1
    {
      if ((plru_bits & (1 << 4)) == 0)  // B4 = 0
      {
        return 2;
      }
      else  // B4 = 1
      {
        return 3;
      }
    }
  }
  else  // B0 = 1
  {
    if ((plru_bits & (1 << 2)) == 0)  // B2 = 0
    {
      if ((plru_bits & (1 << 5)) == 0)  // B5 = 0
      {
        return 4;
      }
      else  // B5 = 1
      {
        return 5;
      }
    }
    else  // B2 = 1
    {
      if ((plru_bits & (1 << 6)) == 0)  // B6 = 0
      {
        return 6;
      }
      else  // B6 = 1
      {
        return 7;
      }
    }
  }
}
// This array is a used to pre-compute which block to replace based on the PLRU bits.
constexpr std::array<u8, NUM_PLRU_BIT_VALUES> WAY_FROM_PLRU = []() ICACHE_CONSTEVAL {
  std::array<u8, NUM_PLRU_BIT_VALUES> result{};
  for (u8 plru_bits = 0; plru_bits < result.size(); plru_bits++)
    result[plru_bits] = WayFromPLRU(plru_bits);
  return result;
}();
#ifdef ICACHE_HAS_CXX_20
static_assert(std::all_of(WAY_FROM_PLRU.begin(), WAY_FROM_PLRU.end(),
                          [](u8 way) { return way < ICACHE_WAYS; }),
              "WAY_FROM_PLRU must reference valid ways only");
#endif
}  // Anonymous namespace

InstructionCache::~InstructionCache()
{
  if (m_config_callback_id)
    Config::RemoveConfigChangedCallback(*m_config_callback_id);
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
  if (!m_config_callback_id)
    m_config_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();

  data.fill({});
  tags.fill({});
  Reset();
}

void InstructionCache::Invalidate(u32 addr)
{
  if (!HID0.ICE || m_disable_icache)
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
  JitInterface::InvalidateICacheLine(addr);
}

u32 InstructionCache::ReadInstruction(u32 addr)
{
  if (!HID0.ICE || m_disable_icache)  // instruction cache is disabled
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
      t = WAY_FROM_VALID[valid[set]];
    else
      t = WAY_FROM_PLRU[plru[set]];
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
  plru[set] = (plru[set] & ~PLRU_RULES[t].first) | PLRU_RULES[t].second;

  const u32 res = Common::swap32(data[set][t][(addr >> 2) & 7]);
  const u32 inmem = Memory::Read_U32(addr);
  if (res != inmem)
  {
    INFO_LOG_FMT(POWERPC,
                 "ICache read at {:08x} returned stale data: CACHED: {:08x} vs. RAM: {:08x}", addr,
                 res, inmem);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::ICACHE_MATTERS);
  }
  return res;
}

void InstructionCache::DoState(PointerWrap& p)
{
  if (p.IsReadMode())
  {
    // Clear valid parts of the lookup tables (this is done instead of using fill(0xff) to avoid
    // loading the entire 4MB of tables into cache)
    for (u32 set = 0; set < ICACHE_SETS; set++)
    {
      for (u32 way = 0; way < ICACHE_WAYS; way++)
      {
        if ((valid[set] & (1 << way)) != 0)
        {
          const u32 addr = (tags[set][way] << 12) | (set << 5);
          if (addr & ICACHE_VMEM_BIT)
            lookup_table_vmem[(addr >> 5) & 0xfffff] = 0xff;
          else if (addr & ICACHE_EXRAM_BIT)
            lookup_table_ex[(addr >> 5) & 0x1fffff] = 0xff;
          else
            lookup_table[(addr >> 5) & 0xfffff] = 0xff;
        }
      }
    }
  }

  p.DoArray(data);
  p.DoArray(tags);
  p.DoArray(plru);
  p.DoArray(valid);

  if (p.IsReadMode())
  {
    // Recompute lookup tables
    for (u32 set = 0; set < ICACHE_SETS; set++)
    {
      for (u32 way = 0; way < ICACHE_WAYS; way++)
      {
        if ((valid[set] & (1 << way)) != 0)
        {
          const u32 addr = (tags[set][way] << 12) | (set << 5);
          if (addr & ICACHE_VMEM_BIT)
            lookup_table_vmem[(addr >> 5) & 0xfffff] = way;
          else if (addr & ICACHE_EXRAM_BIT)
            lookup_table_ex[(addr >> 5) & 0x1fffff] = way;
          else
            lookup_table[(addr >> 5) & 0xfffff] = way;
        }
      }
    }
  }
}

void InstructionCache::RefreshConfig()
{
  m_disable_icache = Config::Get(Config::MAIN_DISABLE_ICACHE);
}
}  // namespace PowerPC
