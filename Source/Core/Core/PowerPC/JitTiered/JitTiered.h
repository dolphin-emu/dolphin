// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <bitset>
#include <vector>

#include "Common/HandShake.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

/// generic path of the Tiered JIT framework.
/// implements an interpreter with block caching and compaction of said cache.
/// JIT-related data and functions are only allowed here if the data
/// has to be managed by the interpreter. (e. g. dispatch cache, invalidation filter)
class JitTieredGeneric : public JitBase
{
public:
  virtual const char* GetName() const { return "TieredGeneric"; }
  virtual void Init() {}
  virtual void ClearCache() final;
  virtual void SingleStep() final;
  virtual void Run();
  virtual void Shutdown() {}

  /// JITs may never reclaim code space while the CPU thread is in JIT code,
  /// so clearing is always safe
  virtual void ClearSafe() final { ClearCache(); }
  virtual bool HandleFault(uintptr_t, SContext*) { return false; }
  virtual void InvalidateICache(u32 address, u32 size, bool forced) final;

protected:
  // for invalidation of JIT blocks
  using Bloom = u64;
  struct DispCacheEntry
  {
    /// PPC instructions are 4-byte aligned, leaving us 2 flag bits with these values:
    /// 0: interpreter (or invalid, if all 0)
    /// 1: Baseline JIT
    /// 2: Optimized JIT
    u32 address;
    /// if interpreter: offset into inst_cache vector
    /// if JIT: offset to JIT code (passed verbatim to entry function)
    u32 offset;
    union
    {
      /// if JIT block
      Bloom bloom;
      /// if interpreter block
      struct
      {
        // number of instructions
        u32 len;
        // number of times the block was entered
        u32 usecount;
      };
    };
  };
  static_assert(sizeof(DispCacheEntry) <= 16, "Dispatch cache entry should fit in 16 bytes");

  typedef void (*InterpreterFunc)(UGeckoInstruction);
  struct DecodedInstruction
  {
    InterpreterFunc func;
    UGeckoInstruction inst;
    /// prefix sum of estimated cycles from start of block
    u32 cycles : 31;
    // whether this instruction causes FPU Unavailable if MSR.FP=0 (not checked in the interpreter
    // functions themselves)
    u32 uses_fpu : 1;
  };
  static_assert(sizeof(DecodedInstruction) <= 16, "Decoded instruction should fit in 16 bytes");

  struct BaselineReport
  {
    struct CompactedBlock
    {
      u32 address;
      u32 start;
      u32 usecount;
    };
    struct Invalidation
    {
      u32 first;
      u32 last;
    };
    /// has sentinel value with address = 0 and start = instructions.size() to make block size
    /// calculation easier
    std::vector<CompactedBlock> blocks;
    std::vector<DecodedInstruction> instructions;
    std::vector<Invalidation> invalidations;
    Bloom invalidation_bloom;
  };

  template <int bits, int shift_off>
  static constexpr u32 XorFold(u32 address)
  {
    address >>= shift_off;
    for (int i = 32 - shift_off; i > 0; i -= bits)
    {
      address = (address >> bits) ^ (address & ((1 << bits) - 1));
    }
    return address;
  }

  static constexpr u32 DispatchCacheKey(u32 addr) { return XorFold<DISP_CACHE_SHIFT, 2>(addr); }

  static Bloom BloomNone() { return 0; }
  static Bloom BloomAll() { return ~BloomNone(); }
  static Bloom BloomRange(u32 first, u32 last)
  {
    // we only have (guest) cacheline resolution (32=2^5 bytes)
    first >>= 5;
    last >>= 5;
    Bloom res = BloomNone();
    while (first <= last)
    {
      res |= (u64)1 << XorFold<6, 0>(first);
      first += 1;
    }
    return res;
  }

  void CompactInterpreterBlocks(bool keep_old_blocks);
  void InterpretBlock(u32);
  void RunZeroInstruction();

  u32 FindBlock(u32 address);
  /// tail-called by FindBlock if it doesn't find a block in the dispatch cache.
  /// *must* create a block of some kind at the index `key` in the dispatch cache, and return `key`.
  /// the default implementation just creates a new interpreter block.
  virtual u32 LookupBlock(u32 key, u32 address);

  /// the least-significant two bits of instruction addresses are always zero, so we can use them
  /// for flags. this constant can be used to mask out the address
  /// (use ~FLAG_MASK to get the address)
  static constexpr u32 FLAG_MASK = 3;

  static constexpr int DISP_CACHE_SHIFT = 8;
  static constexpr size_t DISP_PRIMARY_CACHE_SIZE = 1 << DISP_CACHE_SHIFT;
  static constexpr int VICTIM_SETS_SHIFT = 3;
  static constexpr size_t VICTIM_SETS = 1 << VICTIM_SETS_SHIFT;
  static constexpr int VICTIM_WAYS_SHIFT = 5;
  static constexpr size_t VICTIM_WAYS = 1 << VICTIM_WAYS_SHIFT;
  static constexpr size_t DISP_CACHE_SIZE = DISP_PRIMARY_CACHE_SIZE + VICTIM_SETS * VICTIM_WAYS;

  /// direct-associative cache (hash table) for blocks, followed by victim cache
  /// (contiguous for iteration convenience)
  /// JIT threads may only delete entries, not create them (safety invariant)
  DispCacheEntry disp_cache[DISP_CACHE_SIZE]{};

  /// second-chance bits for the WS-Clock eviction algorithm
  std::bitset<VICTIM_SETS * VICTIM_WAYS> victim_second_chance;
  /// clocks for the WS-Clock eviction algorithm
  u8 victim_clocks[VICTIM_SETS];

  /// contents of interpreter blocks
  std::vector<DecodedInstruction> inst_cache;
  /// offset in inst_cache at which new instructions begin
  size_t offset_new = 0;

  /// interface to Baseline JIT thread.
  /// Defined in the generic class because it is used in compaction and contains the invalidation
  /// bloom filter
  HandShake<BaselineReport> baseline_report;
};
