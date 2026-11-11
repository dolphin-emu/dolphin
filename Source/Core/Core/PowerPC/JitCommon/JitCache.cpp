// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/JitCache.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <ranges>
#include <span>
#include <utility>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/RangeSet.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gen;

static auto covered_macroblocks(const Common::RangeSet<u32>& ranges, u32 step)
{
  return ranges
         // Merge ranges with contigous macroblocks, so that each macroblock is generated only once.
         | std::views::chunk_by([step](auto&& a, auto&& b) {
             const u32 a_last_macroblock = (a.second - 1) / step;
             const u32 b_first_macroblock = (b.first / step);
             return a_last_macroblock >= b_first_macroblock - 1;
           }) |
         std::views::transform([step](auto&& chunk) {
           const u32 start = chunk.front().first;
           const u32 end = chunk.back().second;

           const u32 l = start / step;
           const u32 r = (end - 1) / step;
           return std::views::iota(l, r + 1);
         }) |
         std::views::join;
}

bool JitBlock::OverlapsPhysicalRange(u32 address, u32 length) const
{
  return physical_addresses.overlaps(address, address + length);
}

void JitBlock::ProfileData::BeginProfiling(ProfileData* data)
{
  data->run_count += 1;
  data->time_start = Clock::now();
}

void JitBlock::ProfileData::EndProfiling(ProfileData* data, u32 downcount_amount)
{
  data->cycles_spent += downcount_amount;
  data->time_spent += Clock::now() - data->time_start;
}

JitBaseBlockCache::JitBaseBlockCache(JitBase& jit) : m_jit{jit}
{
}

JitBaseBlockCache::~JitBaseBlockCache() = default;

bool JitBaseBlockCache::Init()
{
  if (!valid_block.IsValid())
  {
    ERROR_LOG_FMT(DYNA_REC, "Failed to create valid_block");
    return false;
  }

  Common::JitRegister::Init(Config::Get(Config::MAIN_PERF_MAP_DIR));

  m_entry_points_ptr = nullptr;
#ifdef _ARCH_64
  if (Config::Get(Config::MAIN_LARGE_ENTRY_POINTS_MAP))
    m_entry_points_ptr = static_cast<u8**>(m_entry_points_arena.Create(FAST_BLOCK_MAP_SIZE));
#endif

  Clear();

  return true;
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
  m_jit.js.fifoWriteAddresses.Clear();
  m_jit.js.pairedQuantizeAddresses.Clear();
  m_jit.js.noSpeculativeConstantsAddresses.Clear();
  for (auto& e : block_map)
  {
    DestroyBlock(e.second);
  }
  block_map.clear();
  links_to.clear();
  block_range_map.clear();

  valid_block.Clear();

  if (m_entry_points_ptr)
    m_entry_points_arena.Clear();
}

bool JitBaseBlockCache::Reset()
{
  Shutdown();
  return Init();
}

u8** JitBaseBlockCache::GetEntryPoints()
{
  return m_entry_points_ptr;
}

JitBlock** JitBaseBlockCache::GetFastBlockMapFallback()
{
  return m_fast_block_map_fallback.data();
}

void JitBaseBlockCache::RunOnBlocks(const Core::CPUThreadGuard&,
                                    const std::function<void(const JitBlock&)>& f) const
{
  for (const auto& e : block_map)
    f(e.second);
}

void JitBaseBlockCache::WipeBlockProfilingData(const Core::CPUThreadGuard&)
{
  for (const auto& kv : block_map)
  {
    if (JitBlock::ProfileData* const profile_data = kv.second.profile_data.get())
      *profile_data = {};
  }
  Host_JitProfileDataWiped();
}

JitBlock JitBaseBlockCache::InitBlock(u32 em_address)
{
  const u32 physical_address = m_jit.m_mmu.JitCache_TranslateAddress(em_address).address;
  JitBlock b(m_jit.IsProfilingEnabled());
  b.effectiveAddress = em_address;
  b.physicalAddress = physical_address;
  b.feature_flags = m_jit.m_ppc_state.feature_flags;
  m_jit.js.link_data_temp.clear();
  return b;
}

void JitBaseBlockCache::FinalizeBlock(JitBlock&& b, bool block_link,
                                      PPCAnalyst::CodeBlock& code_block,
                                      const PPCAnalyst::CodeBuffer& code_buffer)
{
  JitBlock& block =
      block_map.emplace(MapLookupIndex(b.physicalAddress, b.feature_flags), std::move(b))
          .first->second;

  // Copy is intentional (see member's comment).
  block.linkData = m_jit.js.link_data_temp;

  size_t index = FastLookupIndexForAddress(block.effectiveAddress, block.feature_flags);
  if (m_entry_points_ptr)
  {
    m_entry_points_arena.EnsureMemoryPageWritable(index * sizeof(u8*));
    m_entry_points_ptr[index] = block.normalEntry;
  }
  else
  {
    m_fast_block_map_fallback[index] = &block;
  }
  block.fast_block_map_index = index;

  // TODO C++23: If RangeSet starts using flat_map, it would make more sense to copy instead, so
  // that code_block.m_physical_addresses keeps a high capacity, thus minimizing allocations due to
  // exceeded capacity. (Also make code_block const.)
  block.physical_addresses = std::move(code_block.m_physical_addresses);

  block.originalSize = code_block.m_num_instructions;
  if (m_jit.IsDebuggingEnabled())
  {
    // TODO C++23: Can do this all in one statement with `std::vector::assign_range`.
    const std::ranges::transform_view original_buffer_transform_view{
        std::span{code_buffer.data(), block.originalSize},
        [](const PPCAnalyst::CodeOp& op) { return std::make_pair(op.address, op.inst); }};
    block.original_buffer.assign(original_buffer_transform_view.begin(),
                                 original_buffer_transform_view.end());
  }

  for (auto [range_start, range_end] : block.physical_addresses)
  {
    valid_block.SetBits(range_start / VALID_BLOCK_CACHE_LINE_SIZE,
                        Common::DivideRoundingUp(range_end, VALID_BLOCK_CACHE_LINE_SIZE));
  }
  for (u32 i : covered_macroblocks(block.physical_addresses, BLOCK_RANGE_SIZE))
  {
    block_range_map[i].push_back(&block);
  }

  if (block_link)
  {
    for (const auto& e : block.linkData)
    {
      links_to[e.exitAddress].push_back(&block);
    }

    LinkBlock(block);
  }

  const Common::Symbol* symbol = nullptr;
  if (Common::JitRegister::IsEnabled() &&
      (symbol = m_jit.m_ppc_symbol_db.GetSymbolFromAddr(block.effectiveAddress)) != nullptr)
  {
    Common::JitRegister::Register(block.normalEntry, block.near_end - block.normalEntry,
                                  "JIT_PPC_{}_{:08x}", symbol->function_name,
                                  block.physicalAddress);
  }
  else
  {
    Common::JitRegister::Register(block.normalEntry, block.near_end - block.normalEntry,
                                  "JIT_PPC_{:08x}", block.physicalAddress);
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

  auto iter = block_map.find(MapLookupIndex(translated_addr, feature_flags));
  if (iter == block_map.end())
    return nullptr;

  return &iter->second;
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
    if (!valid_block.TestBit(physical_address / VALID_BLOCK_CACHE_LINE_SIZE))
      destroy_block = false;
    else
      valid_block.ClearBit(physical_address / VALID_BLOCK_CACHE_LINE_SIZE);
  }
  else if (length > 32)
  {
    // Even if we can't check the set for optimization, we still want to remove all fully covered
    // cache lines from the valid_block set so that later calls don't try to invalidate already
    // cleared regions.
    const u32 covered_block_start = (physical_address + 0x1f) / VALID_BLOCK_CACHE_LINE_SIZE;
    const u32 covered_block_end = (physical_address + length) / VALID_BLOCK_CACHE_LINE_SIZE;
    valid_block.ClearBits(covered_block_start, covered_block_end);
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
      const size_t start_bit = address / sizeof(UGeckoInstruction);
      const size_t end_bit = (address + length) / sizeof(UGeckoInstruction);
      m_jit.js.fifoWriteAddresses.ClearBits(start_bit, end_bit);
      m_jit.js.pairedQuantizeAddresses.ClearBits(start_bit, end_bit);
      m_jit.js.noSpeculativeConstantsAddresses.ClearBits(start_bit, end_bit);
    }
  }
}

void JitBaseBlockCache::ErasePhysicalRange(u32 address, u32 length)
{
  const u32 start = address / BLOCK_RANGE_SIZE;
  const u32 end = Common::DivideRoundingUp(address + length, BLOCK_RANGE_SIZE);
  // Iterate over all macro blocks which overlap the given range.
  for (u32 i = start; i < end; ++i)
  {
    const auto macroblock = block_range_map.find(i);
    if (macroblock == block_range_map.end())
      continue;

    const auto erase_from_everywhere_else = [&](JitBlock* block) {
      for (u32 j : covered_macroblocks(block->physical_addresses, BLOCK_RANGE_SIZE))
      {
        // This is not the most efficient: if an erasure covers many macroblocks, then the erasure
        // will be one-by-one on the other macroblocks just because they are not the "current" one.
        // But the function is currently low when profiling, so it doesn't matter.
        if (j != i)
        {
          auto macro_it = block_range_map.find(j);
          DEBUG_ASSERT(macro_it != block_range_map.end());
          auto b = std::ranges::find(macro_it->second, block);
          DEBUG_ASSERT(b != macro_it->second.end());
          // Erase b through swap and pop_back (it's more efficient, and we don't care about order).
          std::iter_swap(b, &macro_it->second.back());
          macro_it->second.pop_back();
          // If the macro block is empty, drop it.
          if (macro_it->second.empty())
            block_range_map.erase(macro_it);
        }
      }
      DestroyBlock(*block);
      block_map.erase(MapLookupIndex(block->physicalAddress, block->feature_flags));
    };

    // Iterate over all blocks in the macro block.
    if (i * BLOCK_RANGE_SIZE >= address && (i + 1) * BLOCK_RANGE_SIZE < address + length)
    {
      // The entire macroblock is encompassed by the erasure, no need to check for overlap.
      std::ranges::for_each(macroblock->second, erase_from_everywhere_else);
      block_range_map.erase(macroblock);
    }
    else
    {
      auto to_erase = std::ranges::partition(macroblock->second, [&](JitBlock* block) {
        return !block->OverlapsPhysicalRange(address, length);
      });
      std::ranges::for_each(to_erase, erase_from_everywhere_else);
      macroblock->second.erase(std::ranges::begin(to_erase), std::ranges::end(to_erase));
      // If the macro block is empty, drop it.
      if (macroblock->second.empty())
        block_range_map.erase(macroblock);
    }
  }
}

void JitBaseBlockCache::EraseSingleBlock(const JitBlock& block)
{
  auto iter = block_map.find(MapLookupIndex(block.physicalAddress, block.feature_flags));
  if (iter == block_map.end()) [[unlikely]]
  {
    DEBUG_ASSERT(false);
    return;
  }

  JitBlock& mutable_block = iter->second;

  for (u32 i : covered_macroblocks(mutable_block.physical_addresses, BLOCK_RANGE_SIZE))
  {
    auto macro_it = block_range_map.find(i);
    DEBUG_ASSERT(macro_it != block_range_map.end());
    auto b = std::ranges::find(macro_it->second, &mutable_block);
    DEBUG_ASSERT(b != macro_it->second.end());
    std::iter_swap(b, &macro_it->second.back());
    macro_it->second.pop_back();
    if (macro_it->second.empty())
      block_range_map.erase(macro_it);
  }

  DestroyBlock(mutable_block);
  block_map.erase(iter);  // The original JitBlock reference is now dangling.
}

const u32* JitBaseBlockCache::GetBlockBitSet() const
{
  return valid_block.GetRawMemory();
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
    auto map_it = links_to.find(e.exitAddress);
    if (map_it == links_to.end())
      continue;

    auto vec_it = std::ranges::find(map_it->second, &block);
    if (vec_it == map_it->second.end())
      continue;
    std::iter_swap(vec_it, &map_it->second.back());
    map_it->second.pop_back();
    if (map_it->second.empty())
      links_to.erase(map_it);
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
  {
    m_entry_points_arena.EnsureMemoryPageWritable(index * sizeof(u8*));
    m_entry_points_ptr[index] = block->normalEntry;
  }
  else
  {
    m_fast_block_map_fallback[index] = block;
  }
  block->fast_block_map_index = index;

  return block;
}

u32 JitBaseBlockCache::MapLookupIndex(u32 physical_address, CPUEmuFeatureFlags feature_flags)
{
  // The feature flags are shifted as much as possible because the physical RAM can be extended.
  // TODO: Maybe check that they are indeed disjoint?
  return feature_flags << (32 - std::bit_width(CPUEmuFeatureFlags::FEATURE_FLAG_END_OF_ENUMERATION -
                                               1)) |
         (physical_address >> 2);
}

size_t JitBaseBlockCache::FastLookupIndexForAddress(u32 effective_address,
                                                    CPUEmuFeatureFlags feature_flags)
{
  if (m_entry_points_ptr)
  {
    return (static_cast<size_t>(feature_flags) << 30) | (effective_address >> 2);
  }
  return (effective_address >> 2) & FAST_BLOCK_MAP_FALLBACK_MASK;
}
