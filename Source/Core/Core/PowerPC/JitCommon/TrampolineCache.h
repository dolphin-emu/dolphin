// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/x64Analyzer.h"
#include "Common/x64Emitter.h"

// We need at least this many bytes for backpatching.
const int BACKPATCH_SIZE = 5;

class TrampolineCache : public Gen::X64CodeBlock
{
public:
	void Init();
	void Shutdown();

	const u8 *GetReadTrampoline(const InstructionInfo &info, u32 registersInUse);
	const u8 *GetWriteTrampoline(const InstructionInfo &info, u32 registersInUse, u32 pc);
};
