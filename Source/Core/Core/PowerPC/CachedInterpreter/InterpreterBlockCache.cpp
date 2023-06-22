// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/InterpreterBlockCache.h"

#include "Core/PowerPC/JitCommon/JitBase.h"

BlockCache::BlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
}
