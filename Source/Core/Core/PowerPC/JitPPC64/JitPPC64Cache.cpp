// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/JitPPC64Cache.h"

#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitPPC64/Jit.h"

using namespace PPC64Gen;

JitPPC64BlockCache::JitPPC64BlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

bool JitPPC64BlockCache::Init()
{
  ClearRangesToFree();
  return JitBaseBlockCache::Init();
}

bool JitPPC64BlockCache::CanBranchToBlock(const u8* link, const u8* target)
{
  const s64 offset = target - (link + sizeof(u32));
  return offset >= -(s64{1} << 25) && offset < (s64{1} << 25);
}

void JitPPC64BlockCache::WriteLinkBlock(PPC64XEmitter& emit, const JitBlock::LinkData& source,
                                        const JitBlock* dest)
{
  const u8* start = emit.GetCodePtr();

  if (source.call)
  {
    const u8* slow = start + 3 * sizeof(u32);
    if (dest && CanBranchToBlock(start, dest->normalEntry))
    {
      emit.BC(CCFlags::LE, CRF::CR0, slow);
      emit.BL(dest->normalEntry);
    }
    else
    {
      emit.B(slow);
      emit.NOP();
    }
    emit.B(start + CALL_LINK_SIZE);
  }
  else if (dest && CanBranchToBlock(start, dest->normalEntry))
  {
    const FixupBranch out_of_cycles = emit.BC(CCFlags::LE, CRF::CR0);
    emit.B(dest->normalEntry);
    emit.SetJumpTarget(out_of_cycles);
  }
  else
  {
    emit.NOP();
    emit.NOP();
  }

  const u8* move_start = emit.GetCodePtr();
  emit.MOVI2R(DISPATCHER_PC, source.exitAddress);
  while (emit.GetCodePtr() < move_start + 3 * sizeof(u32) && !emit.HasWriteFailed())
    emit.NOP();
  emit.B(static_cast<const JitPPC64&>(m_jit).Routine(m_jit.GetAsmRoutines()->dispatcher, start));

  ASSERT(emit.GetCodePtr() == start + (source.call ? CALL_LINK_SIZE : BLOCK_LINK_SIZE) ||
         emit.HasWriteFailed());
}

void JitPPC64BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  u8* location = source.exitPtrs;
  PPC64XEmitter emit(location, location + (source.call ? CALL_LINK_SIZE : BLOCK_LINK_SIZE));

  WriteLinkBlock(emit, source, dest);
  emit.FlushIcache();
}

void JitPPC64BlockCache::WriteDestroyBlock(const JitBlock& block)
{
  // Only clear the entry point as we might still be within this block.
  PPC64XEmitter emit(block.normalEntry, block.normalEntry + sizeof(u32));
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  emit.TRAP();
  emit.FlushIcache();
}

void JitPPC64BlockCache::DestroyBlock(JitBlock& block)
{
  JitBaseBlockCache::DestroyBlock(block);

  if (block.near_begin != block.near_end)
    m_ranges_to_free_on_next_codegen.emplace_back(block.near_begin, block.near_end);
  if (block.far_begin != block.far_end)
    m_ranges_to_free_on_next_codegen.emplace_back(block.far_begin, block.far_end);
}

const std::vector<std::pair<u8*, u8*>>& JitPPC64BlockCache::GetRangesToFree() const
{
  return m_ranges_to_free_on_next_codegen;
}

void JitPPC64BlockCache::ClearRangesToFree()
{
  m_ranges_to_free_on_next_codegen.clear();
}
