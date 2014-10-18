// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"

#include "Common/PerformanceCounter.h"

#if defined(_M_X86_64)

#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt) \
	MOV(64, R(ABI_PARAM1), Imm64((u64) pt)); \
	CALL((const void*) QueryPerformanceCounter)

// block->ticCounter += block->ticStop - block->ticStart
#define PROFILER_UPDATE_TIME(block) \
	MOV(64, R(RSCRATCH2), Imm64((u64) block)); \
	MOV(64, R(RSCRATCH), MDisp(RSCRATCH2, offsetof(struct JitBlock, ticStop))); \
	SUB(64, R(RSCRATCH), MDisp(RSCRATCH2, offsetof(struct JitBlock, ticStart))); \
	ADD(64, R(RSCRATCH), MDisp(RSCRATCH2, offsetof(struct JitBlock, ticCounter))); \
	MOV(64, MDisp(RSCRATCH2, offsetof(struct JitBlock, ticCounter)), R(RSCRATCH));

#define PROFILER_VPUSH \
	BitSet32 registersInUse = CallerSavedRegistersInUse(); \
	ABI_PushRegistersAndAdjustStack(registersInUse, 0);

#define PROFILER_VPOP \
	ABI_PopRegistersAndAdjustStack(registersInUse, 0);

#else

#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt)
#define PROFILER_UPDATE_TIME(b)
#define PROFILER_VPUSH
#define PROFILER_VPOP

#endif

struct BlockStat
{
	BlockStat(int bn, u64 c) : blockNum(bn), cost(c) {}
	int blockNum;
	u64 cost;

	bool operator <(const BlockStat &other) const
	{ return cost > other.cost; }
};

namespace Profiler
{
extern bool g_ProfileBlocks;

void WriteProfileResults(const std::string& filename);
}
