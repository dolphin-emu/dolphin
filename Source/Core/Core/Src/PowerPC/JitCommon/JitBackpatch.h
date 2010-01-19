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

#ifndef _JITBACKPATCH_H
#define _JITBACKPATCH_H

#include "Common.h"
#include "x64Emitter.h"
#include "x64Analyzer.h"

// Declarations and definitions
// ----------

// void Jit(u32 em_address);

#ifndef _WIN32

	// A bit of a hack to get things building under linux. We manually fill in this structure as needed
	// from the real context.
	struct CONTEXT
	{
	#ifdef _M_X64
		u64 Rip;
		u64 Rax;
	#else
		u32 Eip;
		u32 Eax;
	#endif 
	};

#endif


class TrampolineCache : public Gen::XCodeBlock
{
public:
	void Init();
	void Shutdown();

	const u8 *GetReadTrampoline(const InstructionInfo &info);
	const u8 *GetWriteTrampoline(const InstructionInfo &info);
};

#endif
