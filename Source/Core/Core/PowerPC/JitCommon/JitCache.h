// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/LazyMemoryRegionBitSet.h"
#include "Common/MemArena.h"
#include "Common/RangeSet.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"

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
  Common::RangeSet<u32> physical_addresses;

  // This is only available when debugging is enabled. It is a trimmed-down copy of the
  // PPCAnalyst::CodeBuffer used to recompile this block, including repeat instructions.
  std::vector<std::pair<u32, UGeckoInstruction>> original_buffer;

  std::unique_ptr<ProfileData> profile_data;
};

typedef void (*CompiledCode)();

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

  [[nodiscard]] virtual bool Init();
  void Shutdown();
  void Clear();
  [[nodiscard]] bool Reset();

  // Code Cache
  u8** GetEntryPoints();
  JitBlock** GetFastBlockMapFallback();
  void RunOnBlocks(const Core::CPUThreadGuard& guard,
                   const std::function<void(const JitBlock&)>& f) const;
  void WipeBlockProfilingData(const Core::CPUThreadGuard& guard);
  std::size_t GetBlockCount() const { return block_map.size(); }

  JitBlock InitBlock(u32 em_address);
  void FinalizeBlock(JitBlock&& b, bool block_link, PPCAnalyst::CodeBlock& code_block,
                     const PPCAnalyst::CodeBuffer& code_buffer);

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
  void EraseSingleBlock(const JitBlock& block);

  const u32* GetBlockBitSet() const;

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

  // Get index for block_map lookup
  static u32 MapLookupIndex(u32 physical_address, CPUEmuFeatureFlags feature_flags);

  // Fast but risky block lookup based on fast_block_map.
  size_t FastLookupIndexForAddress(u32 effective_address, CPUEmuFeatureFlags feature_flags);

  // In a typical game (Mario Kart Wii, branch following enabled) we have, after running the attract
  // screen for a while:
  // - Around 50,000 blocks
  // - Around 30,000 are linked by another block, total entries around 80000.
  // - Around 10,000 ranges are occupied in the block_range_map (BLOCK_RANGE_SIZE = 0x100), total
  // entries around 100,000.

  // links_to hold all exit points of all valid blocks in a reverse way.
  // It is used to query all blocks which link to an address.
  // TODO: In profiling, its percentage is low, but not zero. Replacing std::unordered_map with a
  // faster hashmap wouldn't hurt.
  std::unordered_map<u32, std::vector<JitBlock*>> links_to;  // destination_PC -> number

  // Map indexed by the physical address of the entry point | the shifted MSR.
  // There is no need to involve the effective address: if the BAT is changed, the block cache is
  // completely cleared. If page tables are changed... for performance reasons, we currently assume
  // that no game is so evil to actually use them for instructions. Still, including the effective
  // address in the key wouldn't help, because everything else here relies on effective addresses
  // being stable.
  // TODO: If block linking and fast lookups are disabled, then the above shouldn't be a problem,
  // but it's untested.
  //
  // This is used to query the block based on the current PC in a slow way.
  //
  // The entries are referenced by other members of this struct, so references must be stable upon
  // insertion and erasure (which is the case with std::unordered_map).
  // TODO: Maybe a more performant hashmap, with the value wrapped in unique_ptr for reference
  // stability, may still be more performant? This one is more significant in profiling.
  std::unordered_map<u32, JitBlock> block_map;  // start_addr -> block

  // Range of overlapping code indexed by a masked physical address.
  // This is used for invalidation of memory regions. The range is grouped
  // in macro blocks of each 0x100 bytes.
  static constexpr u32 BLOCK_RANGE_SIZE = 0x100;
  // While an ordered map may grant more performance by iterating through contigous macroblocks,
  // actually taking advantage of it would make the code much more messy.
  // TODO: Like above, a more performant hashmap would be appreciated.
  std::unordered_map<u32, std::vector<JitBlock*>> block_range_map;

  // This bitset shows which cachelines overlap with any blocks.
  // It is used to provide a fast way to query if no icache invalidation is needed.
  static constexpr u32 VALID_BLOCK_CACHE_LINE_SIZE = 32;
  Common::LazyMemoryRegionBitSet valid_block{(1ULL << 32) / VALID_BLOCK_CACHE_LINE_SIZE};

  // This contains the entry points for each block.
  // It is used by the assembly dispatcher to quickly know where to jump based on pc and msr bits.
  Common::LazyMemoryRegion m_entry_points_arena;
  u8** m_entry_points_ptr = nullptr;

  // An alternative for the above but without a shm segment
  // in case the shm memory region couldn't be allocated.
  std::array<JitBlock*, FAST_BLOCK_MAP_FALLBACK_ELEMENTS>
      m_fast_block_map_fallback{};  // start_addr & mask -> number
};
