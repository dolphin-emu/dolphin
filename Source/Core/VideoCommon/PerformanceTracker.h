// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <fstream>
#include <mutex>
#include <optional>

#include "Common/CommonTypes.h"

class PerformanceTracker
{
private:
  // Must be powers of 2 for masking to work
  static constexpr u64 MAX_DT_QUEUE_SIZE = 1UL << 12;
  static constexpr u64 MAX_QUALITY_GRAPH_SIZE = 1UL << 8;

  static inline std::size_t IncrementIndex(const std::size_t index)
  {
    return (index + 1) & (MAX_DT_QUEUE_SIZE - 1);
  }

  static inline std::size_t DecrementIndex(const std::size_t index)
  {
    return (index - 1) & (MAX_DT_QUEUE_SIZE - 1);
  }

  static inline std::size_t GetDifference(const std::size_t begin, const std::size_t end)
  {
    return (end - begin) & (MAX_DT_QUEUE_SIZE - 1);
  }

public:
  PerformanceTracker(const char* log_name = nullptr,
                     const std::optional<s64> sample_window_us = {});
  ~PerformanceTracker();

  PerformanceTracker(const PerformanceTracker&) = delete;
  PerformanceTracker& operator=(const PerformanceTracker&) = delete;
  PerformanceTracker(PerformanceTracker&&) = delete;
  PerformanceTracker& operator=(PerformanceTracker&&) = delete;

  // Functions for recording performance information
  void Reset();
  void Count();

  // Functions for reading performance information
  DT GetSampleWindow() const;

  double GetHzAvg() const;

  DT GetDtAvg() const;
  DT GetDtStd() const;

  DT GetLastRawDt() const;

  void ImPlotPlotLines(const char* label) const;

private:  // Functions for managing dt queue
  inline void QueueClear();
  inline void QueuePush(DT dt);
  inline const DT& QueuePop();
  inline const DT& QueueTop() const;
  inline const DT& QueueBottom() const;

  std::size_t inline QueueSize() const;
  bool inline QueueEmpty() const;

  // Handle pausing and logging
  void LogRenderTimeToFile(DT val);
  void SetPaused(bool paused);

  bool m_paused = false;
  int m_on_state_changed_handle;

  // Name of log file and file stream
  const char* m_log_name;
  std::ofstream m_bench_file;

  // Last time Count() was called
  TimePoint m_last_time;

  // Amount of time to sample dt's over (defaults to config)
  const std::optional<s64> m_sample_window_us;

  // Queue + Running Total used to calculate average dt
  DT m_dt_total = DT::zero();
  std::array<DT, MAX_DT_QUEUE_SIZE> m_dt_queue;
  std::size_t m_dt_queue_begin = 0;
  std::size_t m_dt_queue_end = 0;

  // Average rate/time throughout the window
  DT m_dt_avg = DT::zero();  // Uses Moving Average
  double m_hz_avg = 0.0;     // Uses Moving Average + Euler Average

  // Used to initialize this on demand instead of on every Count()
  mutable std::optional<DT> m_dt_std = std::nullopt;

  // Used to enable thread safety with the performance tracker
  mutable std::mutex m_mutex;
};
