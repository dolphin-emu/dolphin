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
#include "Common/Timer.h"
#include "Core/Core.h"
#include "VideoCommon/VideoConfig.h"

static constexpr double SAMPLE_RC_RATIO = 0.25;

PerformanceTracker::PerformanceTracker(const std::optional<std::string> log_name,
                                       const std::optional<s64> sample_window_us)
    : m_on_state_changed_handle{Core::AddOnStateChangedCallback([this](Core::State state) {
        if (state == Core::State::Paused)
          SetPaused(true);
        else if (state == Core::State::Running)
          SetPaused(false);
      })},
      m_log_name{log_name}, m_sample_window_us{sample_window_us}
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

  QueueClear();
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

  QueuePush(diff);
  m_dt_total += diff;

  if (m_dt_queue_begin == m_dt_queue_end)
    m_dt_total -= QueuePop();

  while (window <= m_dt_total - QueueTop())
    m_dt_total -= QueuePop();

  // Simple Moving Average Throughout the Window
  m_dt_avg = m_dt_total / QueueSize();
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
  return std::chrono::duration_cast<DT>(
      DT_us(m_sample_window_us.value_or(std::max(1, g_ActiveConfig.iPerfSampleUSec))));
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

  if (QueueEmpty())
    return *(m_dt_std = DT::zero());

  double total = 0.0;
  for (std::size_t i = m_dt_queue_begin; i != m_dt_queue_end; i = IncrementIndex(i))
  {
    double diff = DT_s(m_dt_queue[i] - m_dt_avg).count();
    total += diff * diff;
  }

  // This is a weighted standard deviation
  return *(m_dt_std = std::chrono::duration_cast<DT>(DT_s(std::sqrt(total / QueueSize()))));
}

DT PerformanceTracker::GetLastRawDt() const
{
  std::shared_lock lock{m_mutex};

  if (QueueEmpty())
    return DT::zero();

  return QueueBottom();
}

void PerformanceTracker::ImPlotPlotLines(const char* label) const
{
  static std::array<float, MAX_DT_QUEUE_SIZE + 2> x, y;

  std::shared_lock lock{m_mutex};

  if (QueueEmpty())
    return;

  // Decides if there are too many points to plot using rectangles
  const bool quality = QueueSize() < MAX_QUALITY_GRAPH_SIZE;

  const DT update_time = Clock::now() - m_last_time;
  const float predicted_frame_time = DT_ms(std::max(update_time, QueueBottom())).count();

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

  const std::size_t begin = DecrementIndex(m_dt_queue_end);
  const std::size_t end = DecrementIndex(m_dt_queue_begin);
  for (std::size_t i = begin; i != end; i = DecrementIndex(i))
  {
    const float frame_time_ms = DT_ms(m_dt_queue[i]).count();

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

void PerformanceTracker::QueueClear()
{
  m_dt_total = DT::zero();
  m_dt_queue_begin = 0;
  m_dt_queue_end = 0;
}

void PerformanceTracker::QueuePush(DT dt)
{
  m_dt_queue[m_dt_queue_end] = dt;
  m_dt_queue_end = IncrementIndex(m_dt_queue_end);
}

const DT& PerformanceTracker::QueuePop()
{
  const std::size_t top = m_dt_queue_begin;
  m_dt_queue_begin = IncrementIndex(m_dt_queue_begin);
  return m_dt_queue[top];
}

const DT& PerformanceTracker::QueueTop() const
{
  return m_dt_queue[m_dt_queue_begin];
}

const DT& PerformanceTracker::QueueBottom() const
{
  return m_dt_queue[DecrementIndex(m_dt_queue_end)];
}

std::size_t PerformanceTracker::QueueSize() const
{
  return GetDifference(m_dt_queue_begin, m_dt_queue_end);
}

bool PerformanceTracker::QueueEmpty() const
{
  return m_dt_queue_begin == m_dt_queue_end;
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
