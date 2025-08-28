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

  if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
  {
    result = m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed);
  }
  else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
  {
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed);
  }
  else if (type == PQ_BLEND_INPUT)
  {
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed) +
             m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed);
  }
  else if (type == PQ_EFB_COPY_CLOCKS)
  {
    result = m_results[PQG_EFB_COPY_CLOCKS].load(std::memory_order_relaxed);
  }

  return result / 4;
}

bool HardwarePerfQueryBase::IsFlushed() const
{
  return m_query_count.load(std::memory_order_relaxed) == 0;
}
