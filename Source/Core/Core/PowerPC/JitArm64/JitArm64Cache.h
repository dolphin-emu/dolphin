// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/JitCache.h"

typedef void (*CompiledCode)();

class JitArm64BlockCache : public JitBaseBlockCache
{
private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
  void WriteDestroyBlock(const JitBlock& block) override;
};
