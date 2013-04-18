// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _JITBACKPATCH_H
#define _JITBACKPATCH_H

#include "Common.h"
#include "x64Emitter.h"
#include "x64Analyzer.h"
#include "Thunk.h"

// Declarations and definitions
// ----------

// void Jit(u32 em_address);

#ifndef _WIN32

	// A bit of a hack to get things building under linux. We manually fill in this structure as needed
	// from the real context.
	struct CONTEXT
	{
	#ifdef _M_ARM
		u32 reg_pc;
	#else
	#ifdef _M_X64
		u64 Rip;
		u64 Rax;
	#else
		u32 Eip;
		u32 Eax;
	#endif 
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
private:
	ThunkManager thunks;
};

#endif
