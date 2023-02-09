// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <memory>

#include "Common/CommonTypes.h"

enum PerfQueryType
{
  PQ_ZCOMP_INPUT_ZCOMPLOC = 0,
  PQ_ZCOMP_OUTPUT_ZCOMPLOC,
  PQ_ZCOMP_INPUT,
  PQ_ZCOMP_OUTPUT,
  PQ_BLEND_INPUT,
  PQ_EFB_COPY_CLOCKS,
  PQ_NUM_MEMBERS
};

enum PerfQueryGroup
{
  PQG_ZCOMP_ZCOMPLOC,
  PQG_ZCOMP,
  PQG_EFB_COPY_CLOCKS,
  PQG_NUM_MEMBERS,
};

class PerfQueryBase
{
public:
  PerfQueryBase() : m_query_count(0) {}
  virtual ~PerfQueryBase() {}

  virtual bool Initialize() { return true; }

  // Checks if performance queries are enabled in the gameini configuration.
  // NOTE: Called from CPU+GPU thread
  static bool ShouldEmulate();

  // Begin querying the specified value for the following host GPU commands
  // The call to EnableQuery() should be placed immediately before the draw command, otherwise
  // there is a risk of GPU resets if the query is left open and the buffer is submitted during
  // resource binding (D3D12/Vulkan).
  virtual void EnableQuery(PerfQueryGroup type) {}

  // Stop querying the specified value for the following host GPU commands
  virtual void DisableQuery(PerfQueryGroup type) {}

  // Reset query counters to zero and drop any pending queries
  virtual void ResetQuery() {}

  // Return the measured value for the specified query type
  // NOTE: Called from CPU thread
  virtual u32 GetQueryResult(PerfQueryType type) { return 0; }

  // Request the value of any pending queries - causes a pipeline flush and thus should be used
  // carefully!
  virtual void FlushResults() {}

  // True if there are no further pending query results
  // NOTE: Called from CPU thread
  virtual bool IsFlushed() const { return true; }

protected:
  std::atomic<u32> m_query_count;
  std::array<std::atomic<u32>, PQG_NUM_MEMBERS> m_results;
};

extern std::unique_ptr<PerfQueryBase> g_perf_query;
