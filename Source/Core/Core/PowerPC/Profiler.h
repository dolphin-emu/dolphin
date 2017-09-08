// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

struct BlockStat
{
  BlockStat(u32 _addr, u64 c, u64 ticks, u64 run, u32 size)
      : addr(_addr), cost(c), tick_counter(ticks), run_count(run), block_size(size)
  {
  }
  u32 addr;
  u64 cost;
  u64 tick_counter;
  u64 run_count;
  u32 block_size;

  bool operator<(const BlockStat& other) const { return cost > other.cost; }
};
struct ProfileStats
{
  std::vector<BlockStat> block_stats;
  u64 cost_sum;
  u64 timecost_sum;
  u64 countsPerSec;
};

namespace Profiler
{
extern bool g_ProfileBlocks;

void WriteProfileResults(const std::string& filename);
}
