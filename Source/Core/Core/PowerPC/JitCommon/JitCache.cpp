// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/JitCache.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gen;

bool JitBlock::OverlapsPhysicalRange(u32 address, u32 length) const
{
  return physical_addresses.lower_bound(address) !=
         physical_addresses.lower_bound(address + length);
}

JitBaseBlockCache::JitBaseBlockCache(JitBase& jit) : m_jit{jit}
{
}

JitBaseBlockCache::~JitBaseBlockCache() = default;

void JitBaseBlockCache::Init()
{
  Common::JitRegister::Init(Config::Get(Config::MAIN_PERF_MAP_DIR));

#ifdef _ARCH_64
  m_entry_points_ptr = reinterpret_cast<u8**>(m_entry_points_arena.Create(FAST_BLOCK_MAP_SIZE));
#else
  m_entry_points_ptr = nullptr;
#endif

  Clear();
}

void JitBaseBlockCache::Shutdown()
{
  Common::JitRegister::Shutdown();

  m_entry_points_arena.Release();
}

// This clears the JIT cache. It's called from JitCache.cpp when the JIT cache
// is full and when saving and loading states.
void JitBaseBlockCache::Clear()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
  Core::DisplayMessage("Clearing code cache.", 3000);
#endif
  m_jit.js.fifoWriteAddresses.clear();
  m_jit.js.pairedQuantizeAddresses.clear();
  m_jit.js.noSpeculativeConstantsAddresses.clear();
  for (auto& e : block_map)
  {
    DestroyBlock(e.second);
  }
  block_map.clear();
  links_to.clear();
  block_range_map.clear();

  valid_block.ClearAll();

  if (m_entry_points_ptr)
    m_entry_points_arena.Clear();
}

void JitBaseBlockCache::Reset()
{
  Shutdown();
  Init();
}

u8** JitBaseBlockCache::GetEntryPoints()
{
  return m_entry_points_ptr;
}

JitBlock** JitBaseBlockCache::GetFastBlockMapFallback()
{
  return m_fast_block_map_fallback.data();
}

void JitBaseBlockCache::RunOnBlocks(std::function<void(const JitBlock&)> f)
{
  for (const auto& e : block_map)
    f(e.second);
}

JitBlock* JitBaseBlockCache::AllocateBlock(u32 em_address)
{
  const u32 physical_address = m_jit.m_mmu.JitCache_TranslateAddress(em_address).address;
  JitBlock& b = block_map.emplace(physical_address, JitBlock())->second;
  b.effectiveAddress = em_address;
  b.physicalAddress = physical_address;
  b.feature_flags = m_jit.m_ppc_state.feature_flags;
  b.linkData.clear();
  b.fast_block_map_index = 0;
  return &b;
}

void JitBaseBlockCache::FinalizeBlock(JitBlock& block, bool block_link,
                                      const std::set<u32>& physical_addresses)
{
  size_t index = FastLookupIndexForAddress(block.effectiveAddress, block.feature_flags);
  if (m_entry_points_ptr)
    m_entry_points_ptr[index] = block.normalEntry;
  else
    m_fast_block_map_fallback[index] = &block;
  block.fast_block_map_index = index;

  block.physical_addresses = physical_addresses;

  u32 range_mask = ~(BLOCK_RANGE_MAP_ELEMENTS - 1);
  for (u32 addr : physical_addresses)
  {
    valid_block.Set(addr / 32);
    block_range_map[addr & range_mask].insert(&block);
  }

  if (block_link)
  {
    for (const auto& e : block.linkData)
    {
      links_to[e.exitAddress].insert(&block);
    }

    LinkBlock(block);
  }

  Common::Symbol* symbol = nullptr;
  if (Common::JitRegister::IsEnabled() &&
      (symbol = g_symbolDB.GetSymbolFromAddr(block.effectiveAddress)) != nullptr)
  {
    Common::JitRegister::Register(block.normalEntry, block.codeSize, "JIT_PPC_{}_{:08x}",
                                  symbol->function_name.c_str(), block.physicalAddress);
  }
  else
  {
    Common::JitRegister::Register(block.normalEntry, block.codeSize, "JIT_PPC_{:08x}",
                                  block.physicalAddress);
  }
}

JitBlock* JitBaseBlockCache::GetBlockFromStartAddress(u32 addr, CPUEmuFeatureFlags feature_flags)
{
  u32 translated_addr = addr;
  if (feature_flags & FEATURE_FLAG_MSR_IR)
  {
    auto translated = m_jit.m_mmu.JitCache_TranslateAddress(addr);
    if (!translated.valid)
    {
      return nullptr;
    }
    translated_addr = translated.address;
  }

  auto iter = block_map.equal_range(translated_addr);
  for (; iter.first != iter.second; iter.first++)
  {
    JitBlock& b = iter.first->second;
    if (b.effectiveAddress == addr && b.feature_flags == feature_flags)
      return &b;
  }

  return nullptr;
}

const u8* JitBaseBlockCache::Dispatch()
{
  const auto& ppc_state = m_jit.m_ppc_state;
  if (m_entry_points_ptr)
  {
    u8* entry_point =
        m_entry_points_ptr[FastLookupIndexForAddress(ppc_state.pc, ppc_state.feature_flags)];
    if (entry_point)
    {
      return entry_point;
    }
    else
    {
      JitBlock* block = MoveBlockIntoFastCache(ppc_state.pc, ppc_state.feature_flags);

      if (!block)
        return nullptr;

      return block->normalEntry;
    }
  }

  JitBlock* block =
      m_fast_block_map_fallback[FastLookupIndexForAddress(ppc_state.pc, ppc_state.feature_flags)];

  if (!block || block->effectiveAddress != ppc_state.pc ||
      block->feature_flags != ppc_state.feature_flags)
  {
    block = MoveBlockIntoFastCache(ppc_state.pc, ppc_state.feature_flags);
  }

  if (!block)
    return nullptr;

  return block->normalEntry;
}

void JitBaseBlockCache::InvalidateICacheLine(u32 address)
{
  const u32 cache_line_address = address & ~0x1f;
  const auto translated = m_jit.m_mmu.JitCache_TranslateAddress(cache_line_address);
  if (translated.valid)
    InvalidateICacheInternal(translated.address, cache_line_address, 32, false);
}

void JitBaseBlockCache::InvalidateICache(u32 initial_address, u32 initial_length, bool forced)
{
  u32 address = initial_address;
  u32 length = initial_length;
  while (length > 0)
  {
    const auto translated = m_jit.m_mmu.JitCache_TranslateAddress(address);

    const bool address_from_bat = translated.valid && translated.translated && translated.from_bat;
    const int shift = address_from_bat ? PowerPC::BAT_INDEX_SHIFT : PowerPC::HW_PAGE_INDEX_SHIFT;
    const u32 mask = ~((1u << shift) - 1u);
    const u32 first_address = address;
    const u32 last_address = address + (length - 1u);
    if ((first_address & mask) == (last_address & mask))
    {
      if (translated.valid)
        InvalidateICacheInternal(translated.address, address, length, forced);
      return;
    }

    const u32 end_of_page = (first_address + (1u << shift)) & mask;
    const u32 length_this_page = end_of_page - first_address;
    if (translated.valid)
      InvalidateICacheInternal(translated.address, address, length_this_page, forced);
    address = address + length_this_page;
    length = length - length_this_page;
  }
}

void JitBaseBlockCache::InvalidateICacheInternal(u32 physical_address, u32 address, u32 length,
                                                 bool forced)
{
  // Optimization for the case of invalidating a single cache line, which is used by the dcb*
  // instructions. If the valid_block bit for that cacheline is not set, we can safely skip
  // the remaining invalidation logic.
  bool destroy_block = true;
  if (length == 32 && (physical_address & 0x1fu) == 0)
  {
    if (!valid_block.Test(physical_address / 32))
      destroy_block = false;
    else
      valid_block.Clear(physical_address / 32);
  }
  else if (length > 32)
  {
    // Even if we can't check the set for optimization, we still want to remove all fully covered
    // cache lines from the valid_block set so that later calls don't try to invalidate already
    // cleared regions.
    const u32 covered_block_start = (physical_address + 0x1f) / 32;
    const u32 covered_block_end = (physical_address + length) / 32;
    for (u32 i = covered_block_start; i < covered_block_end; ++i)
      valid_block.Clear(i);
  }

  if (destroy_block)
  {
    // destroy JIT blocks
    ErasePhysicalRange(physical_address, length);

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
        m_jit.js.noSpeculativeConstantsAddresses.erase(i);
      }
    }
  }
}

void JitBaseBlockCache::ErasePhysicalRange(u32 address, u32 length)
{
  // Iterate over all macro blocks which overlap the given range.
  u32 range_mask = ~(BLOCK_RANGE_MAP_ELEMENTS - 1);
  auto start = block_range_map.lower_bound(address & range_mask);
  auto end = block_range_map.lower_bound(address + length);
  while (start != end)
  {
    // Iterate over all blocks in the macro block.
    auto iter = start->second.begin();
    while (iter != start->second.end())
    {
      JitBlock* block = *iter;
      if (block->OverlapsPhysicalRange(address, length))
      {
        // If the block overlaps, also remove all other occupied slots in the other macro blocks.
        // This will leak empty macro blocks, but they may be reused or cleared later on.
        for (u32 addr : block->physical_addresses)
          if ((addr & range_mask) != start->first)
            block_range_map[addr & range_mask].erase(block);

        // And remove the block.
        DestroyBlock(*block);
        auto block_map_iter = block_map.equal_range(block->physicalAddress);
        while (block_map_iter.first != block_map_iter.second)
        {
          if (&block_map_iter.first->second == block)
          {
            block_map.erase(block_map_iter.first);
            break;
          }
          block_map_iter.first++;
        }
        iter = start->second.erase(iter);
      }
      else
      {
        iter++;
      }
    }

    // If the macro block is empty, drop it.
    if (start->second.empty())
      start = block_range_map.erase(start);
    else
      start++;
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
  for (auto& e : block.linkData)
  {
    if (!e.linkStatus)
    {
      JitBlock* destinationBlock = GetBlockFromStartAddress(e.exitAddress, block.feature_flags);
      if (destinationBlock)
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
  const auto it = links_to.find(block.effectiveAddress);
  if (it == links_to.end())
    return;

  for (JitBlock* b2 : it->second)
  {
    if (block.feature_flags == b2->feature_flags)
      LinkBlockExits(*b2);
  }
}

void JitBaseBlockCache::UnlinkBlock(const JitBlock& block)
{
  // Unlink all exits of this block.
  for (auto& e : block.linkData)
  {
    WriteLinkBlock(e, nullptr);
  }

  // Unlink all exits of other blocks which points to this block
  const auto it = links_to.find(block.effectiveAddress);
  if (it == links_to.end())
    return;
  for (JitBlock* sourceBlock : it->second)
  {
    if (sourceBlock->feature_flags != block.feature_flags)
      continue;

    for (auto& e : sourceBlock->linkData)
    {
      if (e.exitAddress == block.effectiveAddress)
      {
        WriteLinkBlock(e, nullptr);
        e.linkStatus = false;
      }
    }
  }
}

void JitBaseBlockCache::DestroyBlock(JitBlock& block)
{
  if (m_entry_points_ptr)
  {
    if (m_entry_points_ptr[block.fast_block_map_index] == block.normalEntry)
    {
      m_entry_points_ptr[block.fast_block_map_index] = nullptr;
    }
  }
  else
  {
    if (m_fast_block_map_fallback[block.fast_block_map_index] == &block)
    {
      m_fast_block_map_fallback[block.fast_block_map_index] = nullptr;
    }
  }

  UnlinkBlock(block);

  // Delete linking addresses
  for (const auto& e : block.linkData)
  {
    auto it = links_to.find(e.exitAddress);
    if (it == links_to.end())
      continue;
    it->second.erase(&block);
    if (it->second.empty())
      links_to.erase(it);
  }

  // Raise an signal if we are going to call this block again
  WriteDestroyBlock(block);
}

JitBlock* JitBaseBlockCache::MoveBlockIntoFastCache(u32 addr, CPUEmuFeatureFlags feature_flags)
{
  JitBlock* block = GetBlockFromStartAddress(addr, feature_flags);

  if (!block)
    return nullptr;

  // Drop old fast block map entry
  if (m_entry_points_ptr)
  {
    if (m_entry_points_ptr[block->fast_block_map_index] == block->normalEntry)
    {
      m_entry_points_ptr[block->fast_block_map_index] = nullptr;
    }
  }
  else
  {
    if (m_fast_block_map_fallback[block->fast_block_map_index] == block)
    {
      m_fast_block_map_fallback[block->fast_block_map_index] = nullptr;
    }
  }

  // And create a new one
  size_t index = FastLookupIndexForAddress(addr, feature_flags);
  if (m_entry_points_ptr)
    m_entry_points_ptr[index] = block->normalEntry;
  else
    m_fast_block_map_fallback[index] = block;
  block->fast_block_map_index = index;

  return block;
}

size_t JitBaseBlockCache::FastLookupIndexForAddress(u32 address, u32 feature_flags)
{
  if (m_entry_points_ptr)
  {
    return (feature_flags << 30) | (address >> 2);
  }
  else
  {
    return (address >> 2) & FAST_BLOCK_MAP_FALLBACK_MASK;
  }
}
