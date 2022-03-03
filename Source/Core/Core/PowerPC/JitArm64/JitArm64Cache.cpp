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
  if (!dest)
  {
    // Use a fixed amount of instructions, so we can assume to use 3 instructions on patching.
    emit.MOVZ(DISPATCHER_PC, source.exitAddress & 0xFFFF, ShiftAmount::Shift0);
    emit.MOVK(DISPATCHER_PC, source.exitAddress >> 16, ShiftAmount::Shift16);

    if (source.call)
      emit.BL(m_jit.GetAsmRoutines()->dispatcher);
    else
      emit.B(m_jit.GetAsmRoutines()->dispatcher);
    return;
  }

  if (source.call)
  {
    // The "fast" BL must be the third instruction. So just use the former two to inline the
    // downcount check here. It's better to do this near jump before the long jump to the other
    // block.
    FixupBranch fast_link = emit.B(CC_GT);
    emit.BL(dest->checkedEntry);
    emit.SetJumpTarget(fast_link);
    emit.BL(dest->normalEntry);
    return;
  }

  // Are we able to jump directly to the normal entry?
  s64 distance = ((s64)dest->normalEntry - (s64)emit.GetCodePtr()) >> 2;
  if (distance >= -0x40000 && distance <= 0x3FFFF)
  {
    emit.B(CC_GT, dest->normalEntry);
    emit.B(dest->checkedEntry);
    emit.BRK(101);
    return;
  }

  FixupBranch fast_link = emit.B(CC_GT);
  emit.B(dest->checkedEntry);
  emit.SetJumpTarget(fast_link);
  emit.B(dest->normalEntry);
}

void JitArm64BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  u8* location = source.exitPtrs;
  ARM64XEmitter emit(location, location + 12);

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
