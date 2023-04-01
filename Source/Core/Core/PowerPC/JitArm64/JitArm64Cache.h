// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/JitCache.h"


typedef void (*CompiledCode)();

class JitArm64BlockCache : public JitBaseBlockCache
{
private:
	void WriteLinkBlock(u8* location, const JitBlock& block);
	void WriteDestroyBlock(const u8* location, u32 address);
};
