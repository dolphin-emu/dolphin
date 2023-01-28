// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/LWDProfiler.h"

#include "Core/Core.h"

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"

#include "VideoCommon/VideoConfig.h"

#include <algorithm>
#include <cmath>

static inline bool GetProfilerDisabled()
{
  return !g_ActiveConfig.bEnableProfiler;
}

static LWDProfiler::Profiler s_cpu_profiler;
static LWDProfiler::Profiler s_gpu_profiler;
static LWDProfiler::Profiler s_null_profiler;

LWDProfiler::Profiler& LWDProfiler::GetCPUProfiler()
{
  return s_cpu_profiler;
}

LWDProfiler::Profiler& LWDProfiler::GetGPUProfiler()
{
  return s_gpu_profiler;
}

LWDProfiler::Profiler& LWDProfiler::GetCurrentProfiler()
{
  if (Core::IsCPUThread())
    return GetCPUProfiler();

  if (Core::IsGPUThread())
    return GetGPUProfiler();

  WARN_LOG_FMT(CORE, "Profiler accessed on a non cpu or gpu thread!");
  return s_null_profiler;
}

void LWDProfiler::Profiler::Reset(const Marker first_marker)
{
  std::lock_guard lock{m_profiler_lock};

  m_dt_total = DT::zero();
  m_dt_queue.clear();

  m_dt_marked_totals = {};

  m_last_time = Clock::now();
  m_last_marker = first_marker;
}

void LWDProfiler::Profiler::SetMarker(const Marker next_marker)
{
  if (GetProfilerDisabled())
    return;

  std::lock_guard lock{m_profiler_lock};

  {
    const TimePoint now = Clock::now();
    const DT duration = now - m_last_time;
    m_last_time = now;

    const DT window =
        std::chrono::duration_cast<DT>(DT_us(std::max(1, g_ActiveConfig.iPerfSampleUSec)));

    if (window < duration)
      return Reset(next_marker);

    m_dt_total += duration;
    m_dt_queue.emplace_back(m_dt_marked_totals[m_last_marker], duration);

    while (window < m_dt_total && !m_dt_queue.empty())
    {
      m_dt_total -= m_dt_queue.front().duration;
      m_dt_queue.pop_front();
    }
  }

  {
    m_last_marker = next_marker;
    DT& overhead_total = m_dt_marked_totals["Profiler/Overhead"];

    const TimePoint now = Clock::now();
    const DT duration = now - m_last_time;
    m_last_time = now;

    m_dt_total += duration;
    m_dt_queue.emplace_back(overhead_total, duration);
  }
}

void LWDProfiler::Profiler::PushMarker(const Marker next_marker)
{
  if (GetProfilerDisabled())
    return;

  std::lock_guard lock{m_profiler_lock};

  m_marker_stack.push_back(m_last_marker);
  SetMarker(next_marker);
}

void LWDProfiler::Profiler::PopMarker()
{
  if (GetProfilerDisabled())
    return;

  std::lock_guard lock{m_profiler_lock};

  if (!m_marker_stack.empty())
  {
    SetMarker(m_marker_stack.back());
    m_marker_stack.pop_back();
  }
  else
  {
    SetMarker("Nothing");
  }
}

std::vector<LWDProfiler::Entry> LWDProfiler::Profiler::GetEntries(std::size_t max_entries)
{
  if (GetProfilerDisabled())
    return {};

  std::lock_guard lock{m_profiler_lock};
  ScopedMarker sorting(*this, "Profiler/Overhead");

  std::vector<LWDProfiler::Entry> entries;
  entries.reserve(m_dt_marked_totals.size());

  for (const auto& [marker, duration] : m_dt_marked_totals)
    entries.emplace_back(marker, duration, m_dt_total);

  std::sort(std::begin(entries), std::end(entries), [](auto a, auto b) { return a > b; });

  DT other_duration = DT::zero();
  while (!entries.empty() && max_entries <= entries.size())
  {
    other_duration += entries.back().duration;
    entries.pop_back();
  }

  if (DT::zero() < other_duration)
    entries.emplace_back(Entry("Other", other_duration, m_dt_total));

  return entries;
}
