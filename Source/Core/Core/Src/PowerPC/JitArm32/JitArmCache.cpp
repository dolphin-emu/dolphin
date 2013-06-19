// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include "../JitInterface.h"
#include "Jit.h"
#include "JitArmCache.h"


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


