// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include <cstring>
#include <map>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gen;

bool JitBaseBlockCache::IsFull() const
{
  return GetNumBlocks() >= MAX_NUM_BLOCKS - 1;
}

void JitBaseBlockCache::Init()
{
  JitRegister::Init(SConfig::GetInstance().m_perfDir);

  iCache.fill(0);
  Clear();
}

void JitBaseBlockCache::Shutdown()
{
  num_blocks = 0;

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
  jit->js.fifoWriteAddresses.clear();
  jit->js.pairedQuantizeAddresses.clear();
  for (int i = 0; i < num_blocks; i++)
  {
    DestroyBlock(i, false);
  }
  links_to.clear();
  block_map.clear();

  valid_block.ClearAll();

  num_blocks = 0;
  blocks[0].msrBits = 0xFFFFFFFF;
  blocks[0].invalid = true;
}

void JitBaseBlockCache::Reset()
{
  Shutdown();
  Init();
}

JitBlock* JitBaseBlockCache::GetBlock(int no)
{
  return &blocks[no];
}

int JitBaseBlockCache::GetNumBlocks() const
{
  return num_blocks;
}

int JitBaseBlockCache::AllocateBlock(u32 em_address)
{
  JitBlock& b = blocks[num_blocks];
  b.invalid = false;
  b.effectiveAddress = em_address;
  b.physicalAddress = PowerPC::JitCache_TranslateAddress(em_address).address;
  b.msrBits = MSR & JitBlock::JIT_CACHE_MSR_MASK;
  b.linkData.clear();
  num_blocks++;  // commit the current block
  return num_blocks - 1;
}

void JitBaseBlockCache::FinalizeBlock(int block_num, bool block_link, const u8* code_ptr)
{
  JitBlock& b = blocks[block_num];
  if (start_block_map.count(b.physicalAddress))
  {
    // We already have a block at this address; invalidate the old block.
    // This should be very rare. This will only happen if the same block
    // is called both with DR/IR enabled or disabled.
    WARN_LOG(DYNA_REC, "Invalidating compiled block at same address %08x", b.physicalAddress);
    int old_block_num = start_block_map[b.physicalAddress];
    const JitBlock& old_b = blocks[old_block_num];
    block_map.erase(
        std::make_pair(old_b.physicalAddress + 4 * old_b.originalSize - 1, old_b.physicalAddress));
    DestroyBlock(old_block_num, true);
  }
  start_block_map[b.physicalAddress] = block_num;
  FastLookupEntryForAddress(b.effectiveAddress) = block_num;

  u32 pAddr = b.physicalAddress;

  for (u32 block = pAddr / 32; block <= (pAddr + (b.originalSize - 1) * 4) / 32; ++block)
    valid_block.Set(block);

  block_map[std::make_pair(pAddr + 4 * b.originalSize - 1, pAddr)] = block_num;

  if (block_link)
  {
    for (const auto& e : b.linkData)
    {
      links_to.emplace(e.exitAddress, block_num);
    }

    LinkBlock(block_num);
  }

  JitRegister::Register(b.checkedEntry, b.codeSize, "JIT_PPC_%08x", b.physicalAddress);
}

int JitBaseBlockCache::GetBlockNumberFromStartAddress(u32 addr, u32 msr)
{
  u32 translated_addr = addr;
  if (UReg_MSR(msr).IR)
  {
    auto translated = PowerPC::JitCache_TranslateAddress(addr);
    if (!translated.valid)
    {
      return -1;
    }
    translated_addr = translated.address;
  }

  auto map_result = start_block_map.find(translated_addr);
  if (map_result == start_block_map.end())
    return -1;
  int block_num = map_result->second;
  const JitBlock& b = blocks[block_num];
  if (b.invalid)
    return -1;
  if (b.effectiveAddress != addr)
    return -1;
  if (b.msrBits != (msr & JitBlock::JIT_CACHE_MSR_MASK))
    return -1;
  return block_num;
}

void JitBaseBlockCache::MoveBlockIntoFastCache(u32 addr, u32 msr)
{
  int block_num = GetBlockNumberFromStartAddress(addr, msr);
  if (block_num < 0)
  {
    Jit(addr);
  }
  else
  {
    FastLookupEntryForAddress(addr) = block_num;
    LinkBlock(block_num);
  }
}

const u8* JitBaseBlockCache::Dispatch()
{
  int block_num = FastLookupEntryForAddress(PC);

  while (blocks[block_num].effectiveAddress != PC ||
         blocks[block_num].msrBits != (MSR & JitBlock::JIT_CACHE_MSR_MASK))
  {
    MoveBlockIntoFastCache(PC, MSR & JitBlock::JIT_CACHE_MSR_MASK);
    block_num = FastLookupEntryForAddress(PC);
  }

  return blocks[block_num].normalEntry;
}

// Block linker
// Make sure to have as many blocks as possible compiled before calling this
// It's O(N), so it's fast :)
// Can be faster by doing a queue for blocks to link up, and only process those
// Should probably be done

void JitBaseBlockCache::LinkBlockExits(int i)
{
  JitBlock& b = blocks[i];
  if (b.invalid)
  {
    // This block is dead. Don't relink it.
    return;
  }
  for (auto& e : b.linkData)
  {
    if (!e.linkStatus)
    {
      int destinationBlock = GetBlockNumberFromStartAddress(e.exitAddress, b.msrBits);
      if (destinationBlock != -1)
      {
        if (!blocks[destinationBlock].invalid)
        {
          WriteLinkBlock(e, &blocks[destinationBlock]);
          e.linkStatus = true;
        }
      }
    }
  }
}

void JitBaseBlockCache::LinkBlock(int i)
{
  LinkBlockExits(i);
  const JitBlock& b = blocks[i];
  auto ppp = links_to.equal_range(b.effectiveAddress);

  for (auto iter = ppp.first; iter != ppp.second; ++iter)
  {
    const JitBlock& b2 = blocks[iter->second];
    if (b.msrBits == b2.msrBits)
      LinkBlockExits(iter->second);
  }
}

void JitBaseBlockCache::UnlinkBlock(int i)
{
  JitBlock& b = blocks[i];
  auto ppp = links_to.equal_range(b.effectiveAddress);

  for (auto iter = ppp.first; iter != ppp.second; ++iter)
  {
    JitBlock& sourceBlock = blocks[iter->second];
    if (sourceBlock.msrBits != b.msrBits)
      continue;

    for (auto& e : sourceBlock.linkData)
    {
      if (e.exitAddress == b.effectiveAddress)
      {
        WriteLinkBlock(e, nullptr);
        e.linkStatus = false;
      }
    }
  }
}

void JitBaseBlockCache::DestroyBlock(int block_num, bool invalidate)
{
  if (block_num < 0 || block_num >= num_blocks)
  {
    PanicAlert("DestroyBlock: Invalid block number %d", block_num);
    return;
  }
  JitBlock& b = blocks[block_num];
  if (b.invalid)
  {
    if (invalidate)
      PanicAlert("Invalidating invalid block %d", block_num);
    return;
  }
  b.invalid = true;
  start_block_map.erase(b.physicalAddress);
  FastLookupEntryForAddress(b.effectiveAddress) = 0;

  UnlinkBlock(block_num);

  // Delete linking adresses
  auto it = links_to.equal_range(b.effectiveAddress);
  while (it.first != it.second)
  {
    if (it.first->second == block_num)
      it.first = links_to.erase(it.first);
    else
      it.first++;
  }

  // Raise an signal if we are going to call this block again
  WriteDestroyBlock(b);
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
      DestroyBlock(it->second, true);
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
        jit->js.fifoWriteAddresses.erase(i);
        jit->js.pairedQuantizeAddresses.erase(i);
      }
    }
  }
}

void JitBlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  u8* location = source.exitPtrs;
  const u8* address = dest ? dest->checkedEntry : jit->GetAsmRoutines()->dispatcher;
  XEmitter emit(location);
  if (*location == 0xE8)
  {
    emit.CALL(address);
  }
  else
  {
    // If we're going to link with the next block, there is no need
    // to emit JMP. So just NOP out the gap to the next block.
    // Support up to 3 additional bytes because of alignment.
    s64 offset = address - emit.GetCodePtr();
    if (offset > 0 && offset <= 5 + 3)
      emit.NOP(offset);
    else
      emit.JMP(address, true);
  }
}

void JitBlockCache::WriteDestroyBlock(const JitBlock& block)
{
  // Only clear the entry points as we might still be within this block.
  XEmitter emit((u8*)block.checkedEntry);
  emit.INT3();
  XEmitter emit2((u8*)block.normalEntry);
  emit2.INT3();
}
