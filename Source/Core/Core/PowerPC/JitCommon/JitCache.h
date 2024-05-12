// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <bitset>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Gekko.h"

class JitBase;

// offsetof is only conditionally supported for non-standard layout types,
// so this struct needs to have a standard layout.
struct JitBlockData
{
  // Memory range this code block takes up in near and far code caches.
  u8* near_begin;
  u8* near_end;
  u8* far_begin;
  u8* far_end;

  // The normal entry point for the block, returned by Dispatch().
  u8* normalEntry;

  // The features that this block was compiled with support for.
  CPUEmuFeatureFlags feature_flags;
  // The effective address (PC) for the beginning of the block.
  u32 effectiveAddress;
  // The physical address of the code represented by this block.
  // Various maps in the cache are indexed by this (block_map
  // and valid_block in particular). This is useful because of
  // of the way the instruction cache works on PowerPC.
  u32 physicalAddress;
  // The number of bytes of JIT'ed code contained in this block. Mostly
  // useful for logging.
  u32 codeSize;
  // The number of PPC instructions represented by this block. Mostly
  // useful for logging.
  u32 originalSize;
  // This tracks the position of this block within the fast block cache.
  // We only allow each block to have one map entry.
  size_t fast_block_map_index;
};
static_assert(std::is_standard_layout_v<JitBlockData>, "JitBlockData must have a standard layout");

// A JitBlock is a block of compiled code which corresponds to the PowerPC
// code at a given address.
//
// The notion of the address of a block is a bit complicated because of the
// way address translation works, but basically it's the combination of an
// effective address, the address translation bits in MSR, and the physical
// address.
struct JitBlock : public JitBlockData
{
  // Software profiling data for JIT block.
  struct ProfileData
  {
    using Clock = std::chrono::steady_clock;

    static void BeginProfiling(ProfileData* data);
    static void EndProfiling(ProfileData* data, u32 downcount_amount);

    std::size_t run_count = 0;
    u64 cycles_spent = 0;
    Clock::duration time_spent = {};

  private:
    Clock::time_point time_start;
  };

  explicit JitBlock(bool profiling_enabled)
      : profile_data(profiling_enabled ? std::make_unique<ProfileData>() : nullptr)
  {
  }

  bool OverlapsPhysicalRange(u32 address, u32 length) const;

  // Information about exits to a known address from this block.
  // This is used to implement block linking.
  struct LinkData
  {
    u8* exitPtrs;  // to be able to rewrite the exit jump
#ifdef _M_ARM_64
    const u8* exitFarcode;
#endif
    u32 exitAddress;
    bool linkStatus;  // is it already linked?
    bool call;
  };
  std::vector<LinkData> linkData;

  // This set stores all physical addresses of all occupied instructions.
  std::set<u32> physical_addresses;

  std::unique_ptr<ProfileData> profile_data;
};

typedef void (*CompiledCode)();

// This is essentially just an std::bitset, but Visual Studia 2013's
// implementation of std::bitset is slow.
class ValidBlockBitSet final
{
public:
  enum
  {
    // ValidBlockBitSet covers the whole 32-bit address-space in 32-byte
    // chunks.
    // FIXME: Maybe we can get away with less? There isn't any actual
    // RAM in most of this space.
    VALID_BLOCK_MASK_SIZE = (1ULL << 32) / 32,
    // The number of elements in the allocated array. Each u32 contains 32 bits.
    VALID_BLOCK_ALLOC_ELEMENTS = VALID_BLOCK_MASK_SIZE / 32
  };
  // Directly accessed by Jit64.
  std::unique_ptr<u32[]> m_valid_block;

  ValidBlockBitSet()
  {
    m_valid_block.reset(new u32[VALID_BLOCK_ALLOC_ELEMENTS]);
    ClearAll();
  }

  void Set(u32 bit) { m_valid_block[bit / 32] |= 1u << (bit % 32); }
  void Clear(u32 bit) { m_valid_block[bit / 32] &= ~(1u << (bit % 32)); }
  void ClearAll() { memset(m_valid_block.get(), 0, sizeof(u32) * VALID_BLOCK_ALLOC_ELEMENTS); }
  bool Test(u32 bit) const { return (m_valid_block[bit / 32] & (1u << (bit % 32))) != 0; }
};

class JitBaseBlockCache
{
public:
  // The size of the fast map is determined like this:
  // ((4 GiB guest memory space) / (4-byte alignment) * sizeof(JitBlock*)) << (3 feature flag bits)
  static constexpr u64 FAST_BLOCK_MAP_SIZE = 0x10'0000'0000;
  static constexpr u32 FAST_BLOCK_MAP_FALLBACK_ELEMENTS = 0x10000;
  static constexpr u32 FAST_BLOCK_MAP_FALLBACK_MASK = FAST_BLOCK_MAP_FALLBACK_ELEMENTS - 1;

  explicit JitBaseBlockCache(JitBase& jit);
  virtual ~JitBaseBlockCache();

  virtual void Init();
  void Shutdown();
  void Clear();
  void Reset();

  // Code Cache
  u8** GetEntryPoints();
  JitBlock** GetFastBlockMapFallback();
  void RunOnBlocks(const Core::CPUThreadGuard& guard, std::function<void(const JitBlock&)> f) const;

  JitBlock* AllocateBlock(u32 em_address);
  void FinalizeBlock(JitBlock& block, bool block_link, const std::set<u32>& physical_addresses);

  // Look for the block in the slow but accurate way.
  // This function shall be used if FastLookupIndexForAddress() failed.
  // This might return nullptr if there is no such block.
  JitBlock* GetBlockFromStartAddress(u32 em_address, CPUEmuFeatureFlags feature_flags);

  // Get the normal entry for the block associated with the current program
  // counter. This will JIT code if necessary. (This is the reference
  // implementation; high-performance JITs will want to use a custom
  // assembly version.)
  const u8* Dispatch();

  void InvalidateICache(u32 address, u32 length, bool forced);
  void InvalidateICacheLine(u32 address);
  void ErasePhysicalRange(u32 address, u32 length);

  u32* GetBlockBitSet() const;

protected:
  virtual void DestroyBlock(JitBlock& block);

  JitBase& m_jit;

private:
  virtual void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) = 0;
  virtual void WriteDestroyBlock(const JitBlock& block);

  void LinkBlockExits(JitBlock& block);
  void LinkBlock(JitBlock& block);
  void UnlinkBlock(const JitBlock& block);
  void InvalidateICacheInternal(u32 physical_address, u32 address, u32 length, bool forced);

  JitBlock* MoveBlockIntoFastCache(u32 em_address, CPUEmuFeatureFlags feature_flags);

  // Fast but risky block lookup based on fast_block_map.
  size_t FastLookupIndexForAddress(u32 address, u32 msr);

  // links_to hold all exit points of all valid blocks in a reverse way.
  // It is used to query all blocks which links to an address.
  std::unordered_map<u32, std::unordered_set<JitBlock*>> links_to;  // destination_PC -> number

  // Map indexed by the physical address of the entry point.
  // This is used to query the block based on the current PC in a slow way.
  std::multimap<u32, JitBlock> block_map;  // start_addr -> block

  // Range of overlapping code indexed by a masked physical address.
  // This is used for invalidation of memory regions. The range is grouped
  // in macro blocks of each 0x100 bytes.
  static constexpr u32 BLOCK_RANGE_MAP_ELEMENTS = 0x100;
  std::map<u32, std::unordered_set<JitBlock*>> block_range_map;

  // This bitsets shows which cachelines overlap with any blocks.
  // It is used to provide a fast way to query if no icache invalidation is needed.
  ValidBlockBitSet valid_block;

  // This contains the entry points for each block.
  // It is used by the assembly dispatcher to quickly
  // know where to jump based on pc and msr bits.
  Common::LazyMemoryRegion m_entry_points_arena;
  u8** m_entry_points_ptr = 0;

  // An alternative for the above but without a shm segment
  // in case the shm memory region couldn't be allocated.
  std::array<JitBlock*, FAST_BLOCK_MAP_FALLBACK_ELEMENTS>
      m_fast_block_map_fallback{};  // start_addr & mask -> number
};
