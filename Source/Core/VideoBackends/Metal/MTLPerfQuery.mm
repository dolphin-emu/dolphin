// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLPerfQuery.h"

#include "VideoBackends/Metal/MTLStateTracker.h"

void Metal::PerfQuery::EnableQuery(PerfQueryGroup type)
{
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
    g_state_tracker->EnablePerfQuery(type, m_current_query);
}

void Metal::PerfQuery::DisableQuery(PerfQueryGroup type)
{
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
    g_state_tracker->DisablePerfQuery();
}

void Metal::PerfQuery::ResetQuery()
{
  std::lock_guard<std::mutex> lock(m_results_mtx);
  m_current_query++;
  for (std::atomic<u32>& result : m_results)
    result.store(0, std::memory_order_relaxed);
}

u32 Metal::PerfQuery::GetQueryResult(PerfQueryType type)
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

void Metal::PerfQuery::FlushResults()
{
  if (IsFlushed())
    return;

  // There's a possibility that some active performance queries are unflushed
  g_state_tracker->FlushEncoders();
  g_state_tracker->NotifyOfCPUGPUSync();

  std::unique_lock<std::mutex> lock(m_results_mtx);
  while (!IsFlushed())
    m_cv.wait(lock);
}

bool Metal::PerfQuery::IsFlushed() const
{
  return m_query_count.load(std::memory_order_acquire) == 0;
}

void Metal::PerfQuery::ReturnResults(const u64* data, const PerfQueryGroup* groups, size_t count,
                                     u32 query_id)
{
  {
    std::lock_guard<std::mutex> lock(m_results_mtx);
    if (m_current_query == query_id)
    {
      for (size_t i = 0; i < count; ++i)
      {
        u64 native_res_result =
            data[i] * (EFB_WIDTH * EFB_HEIGHT) /
            (g_framebuffer_manager->GetEFBWidth() * g_framebuffer_manager->GetEFBHeight());

        native_res_result /= g_ActiveConfig.iMultisamples;

        m_results[groups[i]].fetch_add(native_res_result, std::memory_order_relaxed);
      }
    }
    m_query_count.fetch_sub(1, std::memory_order_release);
  }
  m_cv.notify_one();
}
