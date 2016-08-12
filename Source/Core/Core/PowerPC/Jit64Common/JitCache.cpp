// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/JitCache.h"

#include "Core/PowerPC/Jit64Common/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

using namespace Gen;

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
