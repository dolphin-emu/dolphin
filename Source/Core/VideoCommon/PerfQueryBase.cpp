// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerfQueryBase.h"

#include <memory>

#include "VideoCommon/VideoConfig.h"

std::unique_ptr<PerfQueryBase> g_perf_query;

PerfQueryBase::~PerfQueryBase() = default;

bool PerfQueryBase::ShouldEmulate()
{
  return g_ActiveConfig.bPerfQueriesEnable;
}

u32 HardwarePerfQueryBase::GetQueryResult(PerfQueryType type)
{
  u32 result = 0;

  switch (type)
  {
  case PQ_ZCOMP_INPUT_ZCOMPLOC:
  case PQ_ZCOMP_OUTPUT_ZCOMPLOC:
    // Need for Speed: Most Wanted uses this for the sun.
    result = m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed) / 4;
    break;

  case PQ_ZCOMP_INPUT:
  case PQ_ZCOMP_OUTPUT:
    // Timesplitters Future Perfect uses this for the story mode opening cinematic lens flare.
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed);
    break;

  case PQ_BLEND_INPUT:
    // Super Mario Sunshine uses this to complete "Scrubbing Sirena Beach".
    result = (m_results[PQG_ZCOMP].load(std::memory_order_relaxed) +
              m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed)) /
             4;
    break;

  case PQ_EFB_COPY_CLOCKS:
    result = m_results[PQG_EFB_COPY_CLOCKS].load(std::memory_order_relaxed);
    break;

  default:
    break;
  }

  return result;
}

bool HardwarePerfQueryBase::IsFlushed() const
{
  return m_query_count.load(std::memory_order_relaxed) == 0;
}
