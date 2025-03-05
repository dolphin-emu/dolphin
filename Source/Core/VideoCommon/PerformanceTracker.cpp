// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerformanceTracker.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <mutex>

#include <implot.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Core/Core.h"
#include "VideoCommon/VideoConfig.h"

static constexpr double SAMPLE_RC_RATIO = 0.25;
static constexpr u64 MAX_DT_QUEUE_SIZE = 1UL << 12;
static constexpr u64 MAX_QUALITY_GRAPH_SIZE = 1UL << 8;

PerformanceTracker::PerformanceTracker(const std::optional<std::string> log_name,
                                       const std::optional<DT> sample_window_duration)
    : m_on_state_changed_handle{Core::AddOnStateChangedCallback([this](Core::State state) {
        if (state == Core::State::Paused)
          SetPaused(true);
        else if (state == Core::State::Running)
          SetPaused(false);
      })},
      m_log_name{log_name}, m_sample_window_duration{sample_window_duration}
{
  Reset();
}

PerformanceTracker::~PerformanceTracker()
{
  Core::RemoveOnStateChangedCallback(&m_on_state_changed_handle);
}

void PerformanceTracker::Reset()
{
  std::unique_lock lock{m_mutex};

  m_dt_total = DT::zero();
  m_dt_queue.clear();
  m_last_time = Clock::now();
  m_hz_avg = 0.0;
  m_dt_avg = DT::zero();
  m_dt_std = std::nullopt;
}

void PerformanceTracker::Count()
{
  std::unique_lock lock{m_mutex};

  if (m_paused)
    return;

  const DT window{GetSampleWindow()};

  const TimePoint time{Clock::now()};
  const DT diff{time - m_last_time};

  m_last_time = time;

  PushFront(diff);

  if (m_dt_queue.size() == MAX_DT_QUEUE_SIZE)
    PopBack();

  while (m_dt_total - m_dt_queue.back() >= window)
    PopBack();

  // Simple Moving Average Throughout the Window
  m_dt_avg = m_dt_total / m_dt_queue.size();
  const double hz = DT_s(1.0) / m_dt_avg;

  // Exponential Moving Average
  const DT_s rc = SAMPLE_RC_RATIO * std::min(window, m_dt_total);
  const double a = 1.0 - std::exp(-(DT_s(diff) / rc));

  // Sometimes euler averages can break when the average is inf/nan
  if (std::isfinite(m_hz_avg))
    m_hz_avg += a * (hz - m_hz_avg);
  else
    m_hz_avg = hz;

  m_dt_std = std::nullopt;

  LogRenderTimeToFile(diff);
}

DT PerformanceTracker::GetSampleWindow() const
{
  // This reads a constant value and thus does not need a mutex
  return m_sample_window_duration.value_or(
      duration_cast<DT>(DT_us{std::max(1, g_ActiveConfig.iPerfSampleUSec)}));
}

double PerformanceTracker::GetHzAvg() const
{
  std::shared_lock lock{m_mutex};
  return m_hz_avg;
}

DT PerformanceTracker::GetDtAvg() const
{
  std::shared_lock lock{m_mutex};
  return m_dt_avg;
}

DT PerformanceTracker::GetDtStd() const
{
  std::unique_lock lock{m_mutex};

  if (m_dt_std)
    return *m_dt_std;

  if (m_dt_queue.empty())
    return *(m_dt_std = DT::zero());

  double total = 0.0;
  for (auto dt : m_dt_queue)
  {
    double diff = DT_s(dt - m_dt_avg).count();
    total += diff * diff;
  }

  // This is a weighted standard deviation
  return *(m_dt_std = std::chrono::duration_cast<DT>(DT_s(std::sqrt(total / m_dt_queue.size()))));
}

DT PerformanceTracker::GetLastRawDt() const
{
  std::shared_lock lock{m_mutex};

  if (m_dt_queue.empty())
    return DT::zero();

  return m_dt_queue.front();
}

void PerformanceTracker::ImPlotPlotLines(const char* label) const
{
  static std::array<float, MAX_DT_QUEUE_SIZE + 2> x, y;

  std::shared_lock lock{m_mutex};

  if (m_dt_queue.empty())
    return;

  // Decides if there are too many points to plot using rectangles
  const bool quality = m_dt_queue.size() < MAX_QUALITY_GRAPH_SIZE;

  const DT update_time = Clock::now() - m_last_time;
  const float predicted_frame_time = DT_ms(std::max(update_time, m_dt_queue.front())).count();

  std::size_t points = 0;
  if (quality)
  {
    x[points] = 0.f;
    y[points] = predicted_frame_time;
    ++points;
  }

  x[points] = DT_ms(update_time).count();
  y[points] = predicted_frame_time;
  ++points;

  for (auto dt : m_dt_queue)
  {
    const float frame_time_ms = DT_ms(dt).count();

    if (quality)
    {
      x[points] = x[points - 1];
      y[points] = frame_time_ms;
      ++points;
    }

    x[points] = x[points - 1] + frame_time_ms;
    y[points] = frame_time_ms;
    ++points;
  }

  ImPlot::PlotLine(label, x.data(), y.data(), static_cast<int>(points));
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

void PerformanceTracker::SetPaused(bool paused)
{
  std::unique_lock lock{m_mutex};

  m_paused = paused;
  if (m_paused)
  {
    m_last_time = TimePoint::max();
  }
  else
  {
    m_last_time = Clock::now();
  }
}
