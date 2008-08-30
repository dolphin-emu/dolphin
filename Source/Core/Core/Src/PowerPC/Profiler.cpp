// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Jit64/Jit.h"

#include <vector>
#include <algorithm>

#include "SymbolDB.h"

namespace Profiler
{

bool g_ProfileBlocks;
bool g_ProfileInstructions;

struct BlockStat
{
	BlockStat(int bn, int c) : blockNum(bn), cost(c) {}
	int blockNum;
	int cost;

	bool operator <(const BlockStat &other) const {
		return cost > other.cost;
	}
};

void WriteProfileResults(const char *filename) {
	std::vector<BlockStat> stats;
	stats.reserve(Jit64::GetNumBlocks());
	u64 cost_sum = 0;
	for (int i = 0; i < Jit64::GetNumBlocks(); i++)
	{
		const Jit64::JitBlock *block = Jit64::GetBlock(i);
		int cost = (block->originalSize / 4) * block->runCount;  // rough heuristic. mem instructions should cost more.
		if (block->runCount >= 1) {  // Todo: tweak.
			stats.push_back(BlockStat(i, cost));
		}
		cost_sum += cost;
	}

	sort(stats.begin(), stats.end());
	FILE *f = fopen(filename, "w");
	if (!f) {
		PanicAlert("failed to open %s", filename);
		return;
	}
	fprintf(f, "Profile\n");
	for (int i = 0; i < stats.size(); i++)
	{
		const Jit64::JitBlock *block = Jit64::GetBlock(stats[i].blockNum);
		if (block) {
			std::string name = g_symbolDB.GetDescription(block->originalAddress);
			double percent = 100 * (double)stats[i].cost / (double)cost_sum;
			fprintf(f, "%08x - %s - %i (%f%%)\n", block->originalAddress, name.c_str(), stats[i].cost, percent);
		}
	}
	fclose(f);
}

}
