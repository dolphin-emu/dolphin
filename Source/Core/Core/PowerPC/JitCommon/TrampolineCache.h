// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"

// We need at least this many bytes for backpatching.
const int BACKPATCH_SIZE = 5;

class TrampolineCache : public EmuCodeBlock
{
public:
	void Init(int size);
	void Shutdown();

	const u8* GenerateReadTrampoline(const TrampolineInfo &info);
	const u8* GenerateWriteTrampoline(const TrampolineInfo &info);
	void ClearCodeSpace();
};
