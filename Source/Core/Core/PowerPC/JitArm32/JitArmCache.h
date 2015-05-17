// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/JitCache.h"


typedef void (*CompiledCode)();

class JitArmBlockCache : public JitBaseBlockCache
{
private:
	void WriteLinkBlock(u8* location, const u8* address);
	void WriteDestroyBlock(const u8* location, u32 address);
};
