// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/JitArm64Cache.h"

#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

using namespace Arm64Gen;

JitArm64BlockCache::JitArm64BlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void JitArm64BlockCache::Init()
{
  JitBaseBlockCache::Init();
  ClearRangesToFree();
}

void JitArm64BlockCache::WriteLinkBlock(Arm64Gen::ARM64XEmitter& emit,
                                        const JitBlock::LinkData& source, const JitBlock* dest)
{
  const u8* start = emit.GetCodePtr();

  if (!dest)
  {
    emit.MOVI2R(DISPATCHER_PC, source.exitAddress);
    if (source.call)
    {
      while (emit.GetCodePtr() < start + BLOCK_LINK_FAST_BL_OFFSET && !emit.HasWriteFailed())
        emit.NOP();
      emit.BL(m_jit.GetAsmRoutines()->dispatcher);
    }
    else
    {
      emit.B(m_jit.GetAsmRoutines()->dispatcher);
    }
  }
  else
  {
    if (source.call)
    {
      // The "fast" BL should be the last instruction, so that the return address matches the
      // address that was pushed onto the stack by the function that called WriteLinkBlock
      FixupBranch fast = emit.B(CC_GT);
      emit.MOVI2R(DISPATCHER_PC, source.exitAddress);
      emit.BL(m_jit.GetAsmRoutines()->do_timing);
      while (emit.GetCodePtr() < start + BLOCK_LINK_FAST_BL_OFFSET && !emit.HasWriteFailed())
        emit.BRK(101);
      emit.SetJumpTarget(fast);
      emit.BL(dest->normalEntry);
    }
    else
    {
      // Are we able to jump directly to the block?
      s64 block_distance = ((s64)dest->normalEntry - (s64)emit.GetCodePtr()) >> 2;
      if (block_distance >= -0x40000 && block_distance <= 0x3FFFF)
      {
        emit.B(CC_GT, dest->normalEntry);
        emit.MOVI2R(DISPATCHER_PC, source.exitAddress);
        emit.B(m_jit.GetAsmRoutines()->do_timing);
      }
      else
      {
        FixupBranch slow = emit.B(CC_LE);
        emit.B(dest->normalEntry);
        emit.SetJumpTarget(slow);
        emit.MOVI2R(DISPATCHER_PC, source.exitAddress);
        emit.B(m_jit.GetAsmRoutines()->do_timing);
      }
    }
  }

  // Use a fixed number of instructions so we have enough room for any patching needed later.
  const u8* end = start + BLOCK_LINK_SIZE;
  while (emit.GetCodePtr() < end)
  {
    emit.BRK(101);
    if (emit.HasWriteFailed())
      return;
  }
  ASSERT(emit.GetCodePtr() == end);
}

void JitArm64BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  u8* location = source.exitPtrs;
  ARM64XEmitter emit(location, location + BLOCK_LINK_SIZE);

  WriteLinkBlock(emit, source, dest);
  emit.FlushIcache();
}

void JitArm64BlockCache::WriteDestroyBlock(const JitBlock& block)
{
  // Only clear the entry points as we might still be within this block.
  ARM64XEmitter emit(block.checkedEntry, block.normalEntry + 4);
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  while (emit.GetWritableCodePtr() <= block.normalEntry)
    emit.BRK(0x123);
  emit.FlushIcache();
}

void JitArm64BlockCache::DestroyBlock(JitBlock& block)
{
  JitBaseBlockCache::DestroyBlock(block);

  if (block.near_begin != block.near_end)
    m_ranges_to_free_on_next_codegen_near.emplace_back(block.near_begin, block.near_end);
  if (block.far_begin != block.far_end)
    m_ranges_to_free_on_next_codegen_far.emplace_back(block.far_begin, block.far_end);
}

const std::vector<std::pair<u8*, u8*>>& JitArm64BlockCache::GetRangesToFreeNear() const
{
  return m_ranges_to_free_on_next_codegen_near;
}

const std::vector<std::pair<u8*, u8*>>& JitArm64BlockCache::GetRangesToFreeFar() const
{
  return m_ranges_to_free_on_next_codegen_far;
}

void JitArm64BlockCache::ClearRangesToFree()
{
  m_ranges_to_free_on_next_codegen_near.clear();
  m_ranges_to_free_on_next_codegen_far.clear();
}
