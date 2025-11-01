// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerformanceTracker.h"

#include <algorithm>
#include <cmath>
#include <iomanip>

#include <implot.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "VideoCommon/VideoConfig.h"

static constexpr double SAMPLE_RC_RATIO = 0.25;
static constexpr u64 MAX_DT_QUEUE_SIZE = 1UL << 12u;
static constexpr u64 MAX_QUALITY_GRAPH_SIZE = 1UL << 8u;

PerformanceTracker::PerformanceTracker(std::optional<std::string> log_name)
    : m_log_name{std::move(log_name)}
{
  Reset();
}

void PerformanceTracker::Reset()
{
  m_raw_dts.Clear();
  m_dt_queue.clear();

  m_dt_total = DT::zero();
  m_last_raw_dt = DT::zero();
  m_last_time = Clock::now();
  m_hz_avg = 0.0;
  m_dt_avg = DT::zero();
  m_dt_std = DT::zero();
  m_is_last_time_sane = false;
}

void PerformanceTracker::Count()
{
  const TimePoint current_time{Clock::now()};

  if (m_is_last_time_sane.exchange(true, std::memory_order_relaxed))
  {
    const DT diff{current_time - std::exchange(m_last_time, current_time)};
    m_last_raw_dt.store(diff, std::memory_order_relaxed);
    m_raw_dts.Push(diff);
  }
  else
  {
    m_last_time = current_time;
  }
}

void PerformanceTracker::UpdateStats()
{
  DT diff{};
  while (m_raw_dts.Pop(diff))
    HandleRawDt(diff);

  // Update Std Dev
  MathUtil::RunningVariance<double> variance;
  for (auto dt : m_dt_queue)
    variance.Push(DT_s(dt).count());

  const auto dt_std = std::chrono::duration_cast<DT>(DT_s(variance.PopulationStandardDeviation()));
  m_dt_std.store(dt_std, std::memory_order_relaxed);
}

void PerformanceTracker::HandleRawDt(DT diff)
{
  if (m_dt_queue.size() == MAX_DT_QUEUE_SIZE)
    PopBack();

  PushFront(diff);

  const DT window{GetSampleWindow()};

  while (m_dt_total - m_dt_queue.back() >= window)
    PopBack();

  // Simple Moving Average Throughout the Window
  const DT dt_avg = m_dt_total / m_dt_queue.size();
  const double hz = DT_s(1.0) / dt_avg;
  m_dt_avg.store(dt_avg, std::memory_order_relaxed);

  // Exponential Moving Average
  const DT_s rc = SAMPLE_RC_RATIO * std::min(window, m_dt_total);
  const double a = 1.0 - std::exp(-(DT_s(diff) / rc));

  // Sometimes euler averages can break when the average is inf/nan
  auto hz_avg = m_hz_avg.load(std::memory_order_relaxed);
  if (std::isfinite(hz_avg))
    hz_avg = hz_avg + a * (hz - hz_avg);
  else
    hz_avg = hz;
  m_hz_avg.store(hz_avg, std::memory_order_relaxed);

  LogRenderTimeToFile(diff);
}

DT PerformanceTracker::GetSampleWindow()
{
  return duration_cast<DT>(DT_us{std::max(1, g_ActiveConfig.iPerfSampleUSec)});
}

double PerformanceTracker::GetHzAvg() const
{
  return m_hz_avg.load(std::memory_order_relaxed);
}

DT PerformanceTracker::GetDtAvg() const
{
  return m_dt_avg.load(std::memory_order_relaxed);
}

DT PerformanceTracker::GetDtStd() const
{
  return m_dt_std.load(std::memory_order_relaxed);
}

DT PerformanceTracker::GetLastRawDt() const
{
  return m_last_raw_dt.load(std::memory_order_relaxed);
}

void PerformanceTracker::InvalidateLastTime()
{
  m_is_last_time_sane.store(false, std::memory_order_relaxed);
}

void PerformanceTracker::ImPlotPlotLines(const char* label) const
{
  // "quality" graph uses twice as many points.
  static_assert(MAX_QUALITY_GRAPH_SIZE * 2 <= MAX_DT_QUEUE_SIZE);
  static std::array<float, MAX_DT_QUEUE_SIZE + 1> x, y;

  if (m_dt_queue.empty())
    return;

  // Decides if there are too many points to plot using rectangles
  const bool quality = m_dt_queue.size() < MAX_QUALITY_GRAPH_SIZE;

  std::size_t point_index = 0;
  const auto add_point = [&](DT dt, DT shift_x, float prev_ms) {
    const float ms = DT_ms{dt}.count();

    if (quality)
    {
      x[point_index] = prev_ms;
      y[point_index] = ms;
      ++point_index;
    }

    x[point_index] = prev_ms + DT_ms{shift_x}.count();
    y[point_index] = ms;
    ++point_index;
  };

  // add points, right to left.
  for (auto dt : m_dt_queue)
    add_point(dt, dt, x[point_index - 1]);

  ImPlot::PlotLine(label, x.data(), y.data(), static_cast<int>(point_index));
}

void PerformanceTracker::PushFront(DT value)
{
  m_dt_queue.push_front(value);
  m_dt_total += value;
}

void PerformanceTracker::PopBack()
{
  m_dt_total -= m_dt_queue.back();
  m_dt_queue.pop_back();
}

void PerformanceTracker::LogRenderTimeToFile(DT val)
{
  if (!m_log_name || !g_ActiveConfig.bLogRenderTimeToFile)
    return;

  if (!m_bench_file.is_open())
  {
    File::OpenFStream(m_bench_file, File::GetUserPath(D_LOGS_IDX) + *m_log_name,
                      std::ios_base::out);
  }

  m_bench_file << std::fixed << std::setprecision(8) << DT_ms(val).count() << std::endl;
}
