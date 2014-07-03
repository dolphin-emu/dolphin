// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#pragma once

#include <string>

#ifdef _WIN32

#if _M_X86_32
#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt)      \
                    LEA(32, EAX, M(pt)); PUSH(EAX); \
                    CALL(QueryPerformanceCounter)
// TODO: r64 way
// asm write : (u64) dt += t1-t0
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0)  \
                    MOV(32, R(EAX), M(pt1));            \
                    SUB(32, R(EAX), M(pt0));            \
                    MOV(32, R(ECX), M(((u8*)pt1) + 4)); \
                    SBB(32, R(ECX), M(((u8*)pt0) + 4)); \
                    ADD(32, R(EAX), M(pdt));            \
                    MOV(32, R(EDX), M(((u8*)pdt) + 4)); \
                    ADC(32, R(EDX), R(ECX));            \
                    MOV(32, M(pdt), R(EAX));            \
                    MOV(32, M(((u8*)pdt) + 4), R(EDX))

#define PROFILER_VPUSH  PUSH(EAX);PUSH(ECX);PUSH(EDX)
#define PROFILER_VPOP   POP(EDX);POP(ECX);POP(EAX)

#else

#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt)
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0)
#define PROFILER_VPUSH
#define PROFILER_VPOP
#endif

#else
// TODO
#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt)
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
extern bool g_ProfileInstructions;

void WriteProfileResults(const std::string& filename);
}
