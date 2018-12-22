// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/BlockCache.h"

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

JitBlockCache::JitBlockCache(Jit64& jit) : JitBaseBlockCache{jit}, m_jit{jit}
{
}

void JitBlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  Gen::XEmitter emit(source.exitPtrs);
  m_jit.WriteHandoverAtExit(emit, source, dest);
}

void JitBlockCache::WriteDestroyBlock(const JitBlock& block)
{
  // Only clear the entry points as we might still be within this block.
  Gen::XEmitter emit(block.checkedEntry);
  emit.INT3();
  Gen::XEmitter emit2(block.normalEntry);
  emit2.INT3();
}
