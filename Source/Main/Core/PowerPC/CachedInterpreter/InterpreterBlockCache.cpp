// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/CachedInterpreter/InterpreterBlockCache.h"

#include "Core/PowerPC/JitCommon/JitBase.h"

BlockCache::BlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
}
