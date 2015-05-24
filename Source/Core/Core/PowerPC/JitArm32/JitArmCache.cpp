// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArmCache.h"


using namespace ArmGen;

void JitArmBlockCache::WriteLinkBlock(u8* location, const u8* address)
{
	ARMXEmitter emit(location);
	emit.B(address);
	emit.FlushIcache();
}

void JitArmBlockCache::WriteDestroyBlock(const u8* location, u32 address)
{
	ARMXEmitter emit((u8 *)location);
	emit.MOVI2R(R11, address);
	emit.MOVI2R(R12, (u32)jit->GetAsmRoutines()->dispatcher);
	emit.STR(R11, R9, PPCSTATE_OFF(pc));
	emit.B(R12);
	emit.FlushIcache();
}


