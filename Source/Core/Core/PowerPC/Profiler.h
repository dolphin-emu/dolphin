// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#pragma once

#include <string>

#include "Common/CommonTypes.h"

#ifdef _WIN32

#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt) \
	LEA(64, ABI_PARAM1, M(pt)); \
	CALL(QueryPerformanceCounter)

// asm write : (u64) dt += t1-t0
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0) \
	MOV(64, R(RSCRATCH), M(pt1)); \
	SUB(64, R(RSCRATCH), M(pt0)); \
	ADD(64, R(RSCRATCH), M(pdt)); \
	MOV(64, M(pdt), R(RSCRATCH));

#define PROFILER_VPUSH \
	u32 registersInUse = CallerSavedRegistersInUse(); \
	ABI_PushRegistersAndAdjustStack(registersInUse, 0);

#define PROFILER_VPOP \
	ABI_PopRegistersAndAdjustStack(registersInUse, 0);

#else

#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt)

// TODO: Implement generic ways to do this cleanly with all supported architectures
// asm write : (u64) dt += t1-t0
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0)
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
