// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64Cache.h"

void JitArm64BlockCache::WriteLinkBlock(u8* location, const u8* address)
{
	ARM64XEmitter emit(location);
	s64 offset = address - location;

	// different size of the dispatcher call, so they are still continuous
	if (offset > 0 && offset <= 28 && offset % 4 == 0)
	{
		for (int i = 0; i < offset / 4; i++)
			emit.HINT(HINT_NOP);
	}
	else
	{
		emit.B(address);
	}
	emit.FlushIcache();
}

void JitArm64BlockCache::WriteDestroyBlock(const u8* location, u32 address)
{
	// must fit within the code generated in JitArm64::WriteExit
	ARM64XEmitter emit((u8 *)location);
	emit.MOVI2R(W0, address);
	emit.MOVI2R(X30, (u64)jit->GetAsmRoutines()->dispatcher);
	emit.STR(INDEX_UNSIGNED, W0, X29, PPCSTATE_OFF(pc));
	emit.BR(X30);
	emit.FlushIcache();
}

