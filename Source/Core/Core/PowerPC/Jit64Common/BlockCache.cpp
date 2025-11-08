// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64Common/BlockCache.h"

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

JitBlockCache::JitBlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void JitBlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  u8* location = source.exitPtrs;
  const u8* address = dest ? dest->normalEntry : m_jit.GetAsmRoutines()->dispatcher_no_timing_check;
  if (source.call)
  {
    Gen::XEmitter emit(location, location + 5);
    emit.CALL(address);
  }
  else
  {
    // If we're going to link with the next block, there is no need
    // to emit JMP. So just NOP out the gap to the next block.
    // Support up to 3 additional bytes because of alignment.
    s64 offset = address - location;
    if (offset > 0 && offset <= Gen::XEmitter::NEAR_JMP_LEN + 3)
    {
      Gen::XEmitter emit(location, location + offset);
      emit.NOP(offset);
    }
    else
    {
      // Length forced to Near because JMP length might differ between dispatcher and linked block.
      // Technically this isn't necessary, as this is executed after Jit64::JustWriteExit (which
      // also pads), and a Short JMP written on top of a Near JMP isn't incorrect since the garbage
      // bytes are skipped. But they confuse the disassembler, and probably the CPU speculation too.
      Gen::XEmitter emit(location, location + Gen::XEmitter::NEAR_JMP_LEN);
      emit.JMP(address, true);
    }
  }
}

void JitBlockCache::WriteDestroyBlock(const JitBlock& block)
{
  // Only clear the entry point as we might still be within this block.
  Gen::XEmitter emit(block.normalEntry, block.normalEntry + 1);
  emit.INT3();
}

void JitBlockCache::Init()
{
  JitBaseBlockCache::Init();
  ClearRangesToFree();
}

void JitBlockCache::DestroyBlock(JitBlock& block)
{
  JitBaseBlockCache::DestroyBlock(block);

  if (block.near_begin != block.near_end)
    m_ranges_to_free_on_next_codegen_near.emplace_back(block.near_begin, block.near_end);
  if (block.far_begin != block.far_end)
    m_ranges_to_free_on_next_codegen_far.emplace_back(block.far_begin, block.far_end);
}

const std::vector<std::pair<u8*, u8*>>& JitBlockCache::GetRangesToFreeNear() const
{
  return m_ranges_to_free_on_next_codegen_near;
}

const std::vector<std::pair<u8*, u8*>>& JitBlockCache::GetRangesToFreeFar() const
{
  return m_ranges_to_free_on_next_codegen_far;
}

void JitBlockCache::ClearRangesToFree()
{
  m_ranges_to_free_on_next_codegen_near.clear();
  m_ranges_to_free_on_next_codegen_far.clear();
}
