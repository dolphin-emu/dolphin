// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/Jit.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitArm64/JitArm64Cache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitInterface.h"

JitArm64BlockCache::JitArm64BlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void JitArm64BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  u8* location = source.exitPtrs;
  ARM64XEmitter emit(location);

  if (dest)
  {
    if (source.call)
    {
      emit.BL(dest->checkedEntry);
    }
    else
    {
      // Are we able to jump directly to the normal entry?
      s64 distance = ((s64)dest->normalEntry - (s64)location) >> 2;
      if (distance >= -0x40000 && distance <= 0x3FFFF)
      {
        emit.B(CC_PL, dest->normalEntry);
      }

      // Use the checked entry if either downcount is smaller zero,
      // or if we're not able to inline the downcount check here.
      emit.B(dest->checkedEntry);
    }
  }
  else
  {
    emit.MOVI2R(DISPATCHER_PC, source.exitAddress);
    if (source.call)
      emit.BL(m_jit.GetAsmRoutines()->dispatcher);
    else
      emit.B(m_jit.GetAsmRoutines()->dispatcher);
  }
  emit.FlushIcache();
}

void JitArm64BlockCache::WriteDestroyBlock(const JitBlock& block)
{
  // Only clear the entry points as we might still be within this block.
  ARM64XEmitter emit((u8*)block.checkedEntry);

  while (emit.GetWritableCodePtr() <= block.normalEntry)
    emit.BRK(0x123);

  emit.FlushIcache();
}
