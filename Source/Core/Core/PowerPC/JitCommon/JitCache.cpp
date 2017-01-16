// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gen;

static CoreTiming::EventType* s_clear_jit_cache_thread_safe;

static void ClearCacheThreadSafe(u64 userdata, s64 cyclesdata)
{
  JitInterface::ClearCache();
}

JitBaseBlockCache::JitBaseBlockCache(JitBase& jit) : m_jit{jit}
{
}

JitBaseBlockCache::~JitBaseBlockCache() = default;

void JitBaseBlockCache::Init()
{
  s_clear_jit_cache_thread_safe = CoreTiming::RegisterEvent("clearJitCache", ClearCacheThreadSafe);
  JitRegister::Init(SConfig::GetInstance().m_perfDir);

  iCache.fill(nullptr);
  Clear();
}

void JitBaseBlockCache::Shutdown()
{
  num_blocks = 1;

  JitRegister::Shutdown();
}

// This clears the JIT cache. It's called from JitCache.cpp when the JIT cache
// is full and when saving and loading states.
void JitBaseBlockCache::Clear()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
  if (IsFull())
    Core::DisplayMessage("Clearing block cache.", 3000);
  else
    Core::DisplayMessage("Clearing code cache.", 3000);
#endif
  m_jit.js.fifoWriteAddresses.clear();
  m_jit.js.pairedQuantizeAddresses.clear();
  for (int i = 1; i < num_blocks; i++)
  {
    DestroyBlock(blocks[i], false);
  }
  links_to.clear();
  block_map.clear();

  valid_block.ClearAll();

  num_blocks = 1;
  blocks[0].msrBits = 0xFFFFFFFF;
  blocks[0].invalid = true;
}

void JitBaseBlockCache::Reset()
{
  Shutdown();
  Init();
}

void JitBaseBlockCache::SchedulateClearCacheThreadSafe()
{
  CoreTiming::ScheduleEvent(0, s_clear_jit_cache_thread_safe, 0, CoreTiming::FromThread::NON_CPU);
}

bool JitBaseBlockCache::IsFull() const
{
  return num_blocks >= MAX_NUM_BLOCKS - 1;
}

JitBlock** JitBaseBlockCache::GetICache()
{
  return iCache.data();
}

void JitBaseBlockCache::RunOnBlocks(std::function<void(const JitBlock&)> f)
{
  for (int i = 0; i < num_blocks; i++)
    f(blocks[i]);
}

JitBlock* JitBaseBlockCache::AllocateBlock(u32 em_address)
{
  JitBlock& b = blocks[num_blocks];
  b.invalid = false;
  b.effectiveAddress = em_address;
  b.physicalAddress = PowerPC::JitCache_TranslateAddress(em_address).address;
  b.msrBits = MSR & JIT_CACHE_MSR_MASK;
  b.linkData.clear();
  b.in_icache = 0;
  num_blocks++;  // commit the current block
  return &b;
}

void JitBaseBlockCache::FinalizeBlock(JitBlock& block, bool block_link, const u8* code_ptr)
{
  start_block_map.emplace(block.physicalAddress, &block);
  size_t icache = FastLookupEntryForAddress(block.effectiveAddress);
  iCache[icache] = &block;
  block.in_icache = icache;

  u32 pAddr = block.physicalAddress;

  for (u32 addr = pAddr / 32; addr <= (pAddr + (block.originalSize - 1) * 4) / 32; ++addr)
    valid_block.Set(addr);

  block_map.emplace(std::make_pair(pAddr + 4 * block.originalSize - 1, pAddr), &block);

  if (block_link)
  {
    for (const auto& e : block.linkData)
    {
      links_to.emplace(e.exitAddress, &block);
    }

    LinkBlock(block);
  }

  JitRegister::Register(block.checkedEntry, block.codeSize, "JIT_PPC_%08x", block.physicalAddress);
}

JitBlock* JitBaseBlockCache::GetBlockFromStartAddress(u32 addr, u32 msr)
{
  u32 translated_addr = addr;
  if (UReg_MSR(msr).IR)
  {
    auto translated = PowerPC::JitCache_TranslateAddress(addr);
    if (!translated.valid)
    {
      return nullptr;
    }
    translated_addr = translated.address;
  }

  auto iter = start_block_map.equal_range(translated_addr);
  for (; iter.first != iter.second; iter.first++)
  {
    JitBlock& b = *iter.first->second;
    if (!b.invalid && b.effectiveAddress == addr && b.msrBits == (msr & JIT_CACHE_MSR_MASK))
      return &b;
  }

  return nullptr;
}

const u8* JitBaseBlockCache::Dispatch()
{
  JitBlock* block = iCache[FastLookupEntryForAddress(PC)];

  while (!block || block->effectiveAddress != PC || block->msrBits != (MSR & JIT_CACHE_MSR_MASK))
  {
    MoveBlockIntoFastCache(PC, MSR & JIT_CACHE_MSR_MASK);
    block = iCache[FastLookupEntryForAddress(PC)];
  }

  return block->normalEntry;
}

void JitBaseBlockCache::InvalidateICache(u32 address, const u32 length, bool forced)
{
  auto translated = PowerPC::JitCache_TranslateAddress(address);
  if (!translated.valid)
    return;
  u32 pAddr = translated.address;

  // Optimize the common case of length == 32 which is used by Interpreter::dcb*
  bool destroy_block = true;
  if (length == 32)
  {
    if (!valid_block.Test(pAddr / 32))
      destroy_block = false;
    else
      valid_block.Clear(pAddr / 32);
  }

  // destroy JIT blocks
  // !! this works correctly under assumption that any two overlapping blocks end at the same
  // address
  if (destroy_block)
  {
    auto it = block_map.lower_bound(std::make_pair(pAddr, 0));
    while (it != block_map.end() && it->first.second < pAddr + length)
    {
      DestroyBlock(*it->second, true);
      it = block_map.erase(it);
    }

    // If the code was actually modified, we need to clear the relevant entries from the
    // FIFO write address cache, so we don't end up with FIFO checks in places they shouldn't
    // be (this can clobber flags, and thus break any optimization that relies on flags
    // being in the right place between instructions).
    if (!forced)
    {
      for (u32 i = address; i < address + length; i += 4)
      {
        m_jit.js.fifoWriteAddresses.erase(i);
        m_jit.js.pairedQuantizeAddresses.erase(i);
      }
    }
  }
}

u32* JitBaseBlockCache::GetBlockBitSet() const
{
  return valid_block.m_valid_block.get();
}

void JitBaseBlockCache::WriteDestroyBlock(const JitBlock& block)
{
}

// Block linker
// Make sure to have as many blocks as possible compiled before calling this
// It's O(N), so it's fast :)
// Can be faster by doing a queue for blocks to link up, and only process those
// Should probably be done

void JitBaseBlockCache::LinkBlockExits(JitBlock& block)
{
  if (block.invalid)
  {
    // This block is dead. Don't relink it.
    return;
  }
  for (auto& e : block.linkData)
  {
    if (!e.linkStatus)
    {
      JitBlock* destinationBlock = GetBlockFromStartAddress(e.exitAddress, block.msrBits);
      if (destinationBlock && !destinationBlock->invalid)
      {
        WriteLinkBlock(e, destinationBlock);
        e.linkStatus = true;
      }
    }
  }
}

void JitBaseBlockCache::LinkBlock(JitBlock& block)
{
  LinkBlockExits(block);
  auto ppp = links_to.equal_range(block.effectiveAddress);

  for (auto iter = ppp.first; iter != ppp.second; ++iter)
  {
    JitBlock& b2 = *iter->second;
    if (block.msrBits == b2.msrBits)
      LinkBlockExits(b2);
  }
}

void JitBaseBlockCache::UnlinkBlock(const JitBlock& block)
{
  auto ppp = links_to.equal_range(block.effectiveAddress);

  for (auto iter = ppp.first; iter != ppp.second; ++iter)
  {
    JitBlock& sourceBlock = *iter->second;
    if (sourceBlock.msrBits != block.msrBits)
      continue;

    for (auto& e : sourceBlock.linkData)
    {
      if (e.exitAddress == block.effectiveAddress)
      {
        WriteLinkBlock(e, nullptr);
        e.linkStatus = false;
      }
    }
  }
}

void JitBaseBlockCache::DestroyBlock(JitBlock& block, bool invalidate)
{
  if (block.invalid)
  {
    if (invalidate)
      PanicAlert("Invalidating invalid block %p", &block);
    return;
  }
  block.invalid = true;
  if (iCache[block.in_icache] == &block)
    iCache[block.in_icache] = nullptr;

  auto iter = start_block_map.equal_range(block.physicalAddress);
  while (iter.first != iter.second)
  {
    if (iter.first->second == &block)
      iter.first = start_block_map.erase(iter.first);
    else
      iter.first++;
  }

  UnlinkBlock(block);

  // Delete linking addresses
  for (const auto& e : block.linkData)
  {
    auto it = links_to.equal_range(e.exitAddress);
    while (it.first != it.second)
    {
      if (it.first->second == &block)
        it.first = links_to.erase(it.first);
      else
        it.first++;
    }
  }

  // Raise an signal if we are going to call this block again
  WriteDestroyBlock(block);
}

void JitBaseBlockCache::MoveBlockIntoFastCache(u32 addr, u32 msr)
{
  JitBlock* block = GetBlockFromStartAddress(addr, msr);
  if (!block)
  {
    Jit(addr);
  }
  else
  {
    // Drop old icache entry
    if (iCache[block->in_icache] == block)
      iCache[block->in_icache] = nullptr;

    // And create a new one
    size_t icache = FastLookupEntryForAddress(addr);
    iCache[icache] = block;
    block->in_icache = icache;
    LinkBlock(*block);
  }
}

size_t JitBaseBlockCache::FastLookupEntryForAddress(u32 address)
{
  return (address >> 2) & iCache_Mask;
}
