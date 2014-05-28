// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#pragma once

#include <string>

#define PROFILER_QUERY_PERFORMANCE_COUNTER(pt)

// TODO: Implement generic ways to do this cleanly with all supported architectures
// asm write : (u64) dt += t1-t0
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0)
#define PROFILER_VPUSH
#define PROFILER_VPOP

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
