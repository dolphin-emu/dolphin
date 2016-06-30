// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <map>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

// A JitBlock is block of compiled code which corresponds to the PowerPC
// code at a given address.
//
// The notion of the address of a block is a bit complicated because of the
// way address translation works, but basically it's the combination of an
// effective address, the address translation bits in MSR, and the physical
// address.
struct JitBlock
{
  enum
  {
    // Mask for the MSR bits which determine whether a compiled block
    // is valid (MSR.IR and MSR.DR, the address translation bits).
    JIT_CACHE_MSR_MASK = 0x30,
  };

  // A special entry point for block linking; usually used to check the
  // downcount.
  const u8* checkedEntry;
  // The normal entry point for the block, returned by Dispatch().
  const u8* normalEntry;

  // The effective address (PC) for the beginning of the block.
  u32 effectiveAddress;
  // The MSR bits expected for this block to be valid; see JIT_CACHE_MSR_MASK.
  u32 msrBits;
  // The physical address of the code represented by this block.
  // Various maps in the cache are indexed by this (start_block_map,
  // block_map, and valid_block in particular).  This is useful because of
  // of the way the instruction cache works on PowerPC.
  u32 physicalAddress;
  // The number of bytes of JIT'ed code contained in this block. Mostly
  // useful for logging.
  u32 codeSize;
  // The number of PPC instructions represented by this block.  Mostly
  // useful for logging.
  u32 originalSize;
  int runCount;  // for profiling.

  // Whether this struct refers to a valid block.  This is mostly useful as
  // a debugging aid.
  // FIXME: Change current users of invalid bit to assertions?
  bool invalid;

  // Information about exits to a known address from this block.
  // This is used to implement block linking.
  struct LinkData
  {
    u8* exitPtrs;  // to be able to rewrite the exit jump
    u32 exitAddress;
    bool linkStatus;  // is it already linked?
  };
  std::vector<LinkData> linkData;

  // we don't really need to save start and stop
  // TODO (mb2): ticStart and ticStop -> "local var" mean "in block" ... low priority ;)
  u64 ticStart;    // for profiling - time.
  u64 ticStop;     // for profiling - time.
  u64 ticCounter;  // for profiling - time.
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
    // FIXME: Maybe we can get away with less?  There isn't any actual
    // RAM in most of this space.
    VALID_BLOCK_MASK_SIZE = (1ULL << 32) / 32,
    // The number of elements in the allocated array.  Each u32 contains 32 bits.
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
  bool Test(u32 bit) { return (m_valid_block[bit / 32] & (1u << (bit % 32))) != 0; }
};

class JitBaseBlockCache
{
  enum
  {
    MAX_NUM_BLOCKS = 65536 * 2,
  };

  std::array<JitBlock, MAX_NUM_BLOCKS> blocks;
  int num_blocks;
  std::multimap<u32, int> links_to;
  std::map<std::pair<u32, u32>, u32> block_map;  // (end_addr, start_addr) -> number
  std::map<u32, u32> start_block_map;            // start_addr -> number
  ValidBlockBitSet valid_block;

  void LinkBlockExits(int i);
  void LinkBlock(int i);
  void UnlinkBlock(int i);

  void DestroyBlock(int block_num, bool invalidate);

  // Virtual for overloaded
  virtual void WriteLinkBlock(u8* location, const JitBlock& block) = 0;
  virtual void WriteDestroyBlock(const u8* location, u32 address) = 0;
  virtual void WriteUndestroyBlock(const u8* location, u32 address) = 0;

public:
  JitBaseBlockCache() : num_blocks(0) {}
  virtual ~JitBaseBlockCache() {}
  int AllocateBlock(u32 em_address);
  void FinalizeBlock(int block_num, bool block_link, const u8* code_ptr);

  void Clear();
  void Init();
  void Shutdown();
  void Reset();

  bool IsFull() const;

  // Code Cache
  JitBlock* GetBlock(int block_num);
  JitBlock* GetBlocks() { return blocks.data(); }
  int GetNumBlocks() const;
  static const u32 iCache_Num_Elements = 0x10000;
  static const u32 iCache_Mask = iCache_Num_Elements - 1;
  std::array<int, iCache_Num_Elements> iCache;
  int& FastLookupEntryForAddress(u32 address) { return iCache[(address >> 2) & iCache_Mask]; }

  // Fast way to get a block. Only works on the first ppc instruction of a block.
  int GetBlockNumberFromStartAddress(u32 em_address);

  void MoveBlockIntoFastCache(u32 em_address);

  // Get the normal entry for the block associated with the current program
  // counter. This will JIT code if necessary. (This is the reference
  // implementation; high-performance JITs will want to use a custom
  // assembly version.)
  const u8* Dispatch();

  void InvalidateICache(u32 address, const u32 length, bool forced);

  u32* GetBlockBitSet() const { return valid_block.m_valid_block.get(); }
  void EvictTLBEntry(u32 address);
};

// x86 BlockCache
class JitBlockCache : public JitBaseBlockCache
{
private:
  void WriteLinkBlock(u8* location, const JitBlock& block) override;
  void WriteDestroyBlock(const u8* location, u32 address) override;
  void WriteUndestroyBlock(const u8* location, u32 address) override;
};
