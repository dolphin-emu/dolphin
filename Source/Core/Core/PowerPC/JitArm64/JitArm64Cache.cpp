// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64Cache.h"

void JitArm64BlockCache::WriteLinkBlock(u8* location, const JitBlock& block)
{
	ARM64XEmitter emit(location);

	// Are we able to jump directly to the normal entry?
	s64 distance = ((s64)block.normalEntry - (s64)location) >> 2;
	if (distance >= -0x40000 && distance <= 0x3FFFF)
	{
		emit.B(CC_LE, block.normalEntry);

		// We can't write DISPATCHER_PC here, as blink linking is only for 8bytes.
		// So we'll hit two jumps when calling Advance.
		emit.B(block.checkedEntry);
	}
	else
	{
		emit.B(block.checkedEntry);
	}
	emit.FlushIcache();
}

void JitArm64BlockCache::WriteDestroyBlock(const u8* location, u32 address)
{
	// must fit within the code generated in JitArm64::WriteExit
	ARM64XEmitter emit((u8 *)location);
	emit.MOVI2R(DISPATCHER_PC, address);
	emit.B(jit->GetAsmRoutines()->dispatcher);
	emit.FlushIcache();
}

