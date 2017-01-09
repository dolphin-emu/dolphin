// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/CachedInterpreter/InterpreterBlockCache.h"

BlockCache::BlockCache() = default;

void BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
}
