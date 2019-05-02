// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/BlockCache.h"

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

JitBlockCache::JitBlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void JitBlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  // Do not skip breakpoint check if debugging.
  const u8* dispatcher = SConfig::GetInstance().bEnableDebugging ?
                             m_jit.GetAsmRoutines()->dispatcher :
                             m_jit.GetAsmRoutines()->dispatcher_no_check;

  u8* location = source.exitPtrs;
  const u8* address = dest ? dest->checkedEntry : dispatcher;
  Gen::XEmitter emit(location);
  if (source.call)
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
  Gen::XEmitter emit(block.checkedEntry);
  emit.INT3();
  Gen::XEmitter emit2(block.normalEntry);
  emit2.INT3();
}
