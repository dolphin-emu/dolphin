// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/PPCCache.h"

#include <array>

#include "Common/BitUtils.h"
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
constexpr u32 BLOCK_NUM_BITS = 5;
constexpr u32 BLOCK_SIZE_BYTES = ICACHE_BLOCK_SIZE_WORDS * sizeof(u32);  // 0x20
constexpr u32 BLOCK_MASK = BLOCK_SIZE_BYTES - 1;                         // 0x1f
static_assert(1 << BLOCK_NUM_BITS == BLOCK_SIZE_BYTES);

constexpr u32 SET_NUM_BITS = 7;
constexpr u32 SET_SHIFT = BLOCK_NUM_BITS;
constexpr u32 SET_MASK = ICACHE_SETS - 1;  // 0x7f
static_assert(1 << SET_NUM_BITS == ICACHE_SETS);

// The tag is composed of the remaining 20 bits not used to identify the block or set.
// The tag is a physical address, while the block offset and set index can come from the effective
// address or physical address, as address translation will not change them. (Real hardware does
// address translation in parallel with set selection, but we can just use the physical address for
// set index.) See section 3.2 Instruction Cache Organization on page 128 of the 750cl manual.
// Note that the tag could be determined in two ways: shifting right by 12 bits (to disacrd the
// non-tag bits), or masking off the bottom 12 bits. We do the latter to make debugging easier since
// then the tag corresponds to the physical address without needing to shift back.
constexpr u32 NON_TAG_NUM_BITS = BLOCK_NUM_BITS + SET_NUM_BITS;  // 12
constexpr u32 TAG_NUM_BITS = 32 - NON_TAG_NUM_BITS;              // 20
constexpr u32 TAG_MASK = ((1 << TAG_NUM_BITS) - 1) << NON_TAG_NUM_BITS;

constexpr u8 ALL_WAYS_VALID = 0b11111111;
static_assert(ALL_WAYS_VALID == (1 << ICACHE_WAYS) - 1);

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

void InstructionCache::Invalidate(const u32 addr)
{
  if (!HID0.ICE || m_disable_icache)
    return;

  // Per the 750cl manual, section 3.4.1.5 Instruction Cache Enabling/Disabling (page 137)
  // and section 3.4.2.6 Instruction Cache Block Invalidate (icbi) (page 140), the icbi
  // instruction always invalidates, even if the instruction cache is disabled or locked,
  // and it also invalidates all ways of the corresponding cache set, not just the way corresponding
  // to the given address. This also means that the address does not *actually* need to be converted
  // to a physical address. Currently, Interpreter::icbi does not do this conversion.
  // (However, the icbi instruction's info on page 432 does not include this information)
  const u32 set = (addr >> SET_SHIFT) & SET_MASK;
  valid[set] = 0b00000000;
  // Also tell the JIT that the corresponding address has been invalidated. This function takes a
  // virtual address.
  JitInterface::InvalidateICacheLine(addr);
}

u32 InstructionCache::ReadInstruction(const u32 physical_addr)
{
  if (!HID0.ICE || m_disable_icache)  // instruction cache is disabled
    return Memory::Read_U32(physical_addr);

  const u32 set = (physical_addr >> SET_SHIFT) & SET_MASK;
  const u32 tag = physical_addr & TAG_MASK;
  const u32 block_offset_bytes = physical_addr & BLOCK_MASK;
  const u32 block_offset_words = block_offset_bytes >> 2;  // assume already word-aligned

  for (u32 way = 0; way < ICACHE_WAYS; way++)
  {
    if (tags[set][way] == tag && (valid[set] & (1 << way)) != 0)
    {
      // We found a valid cache way that matches the tag, so it contains data corresponding to the
      // input address.
      const u32 res = ReadFromBlock(set, way, block_offset_words);

      // Verify the result, to determine if icache was actually needed.
      // (This is not something that happens on real hardware.)
      const u32 inmem = Memory::Read_U32(physical_addr);
      if (res != inmem)
      {
        INFO_LOG_FMT(POWERPC,
                     "ICache read at {:08x} returned stale data: CACHED: {:08x} vs. RAM: {:08x}",
                     physical_addr, res, inmem);
        DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::ICACHE_MATTERS);
      }

      return res;
    }
  }

  // We failed to find a matching cache way. Load data into a new way, instead.
  if (HID0.ILOCK)  // instruction cache is locked
    return Memory::Read_U32(physical_addr);

  // Select a way
  u32 way;
  if (valid[set] != ALL_WAYS_VALID)
  {
    // There is a free way available, so we use that one.
    way = WAY_FROM_VALID[valid[set]];
  }
  else
  {
    // All ways contain valid data, so use the pseudo least recently used logic to pick the one to
    // replace.
    way = WAY_FROM_PLRU[plru[set]];
  }

  // load
  Memory::CopyFromEmu(reinterpret_cast<u8*>(data[set][way].data()), (physical_addr & ~BLOCK_MASK),
                      BLOCK_SIZE_BYTES);
  tags[set][way] = tag;
  valid[set] |= (1 << way);

  return ReadFromBlock(set, way, block_offset_words);
  // It's impossible for stale data to be read in this case.
}

u32 InstructionCache::ReadFromBlock(const u32 set, const u32 way, const u32 block_offset_words)
{
  // update plru
  plru[set] = (plru[set] & ~PLRU_RULES[way].first) | PLRU_RULES[way].second;

  const u32 res = Common::swap32(data[set][way][block_offset_words]);

  return res;
}

void InstructionCache::DoState(PointerWrap& p)
{
  p.DoArray(data);
  p.DoArray(tags);
  p.DoArray(plru);
  p.DoArray(valid);
}

void InstructionCache::RefreshConfig()
{
  m_disable_icache = Config::Get(Config::MAIN_DISABLE_ICACHE);
}
}  // namespace PowerPC
