// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <fstream>
#include <optional>
#include <shared_mutex>

#include "Common/CommonTypes.h"

class PerformanceTracker
{
private:
  // Must be powers of 2 for masking to work
  static constexpr u64 MAX_DT_QUEUE_SIZE = 1UL << 13;
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

  struct TimeDataPair
  {
  public:
    TimeDataPair(DT duration, DT value) : duration{duration}, value{value} {}
    TimeDataPair(DT duration) : TimeDataPair{duration, duration} {}
    TimeDataPair() : TimeDataPair{DT::zero()} {}

    TimeDataPair& operator+=(const TimeDataPair& other)
    {
      duration += other.duration;
      value += other.value;
      return *this;
    }

    TimeDataPair& operator-=(const TimeDataPair& other)
    {
      duration -= other.duration;
      value -= other.value;
      return *this;
    }

  public:
    DT duration, value;
  };

public:
  PerformanceTracker(const std::optional<std::string> log_name = std::nullopt,
                     const std::optional<s64> sample_window_us = std::nullopt);
  ~PerformanceTracker();

  PerformanceTracker(const PerformanceTracker&) = delete;
  PerformanceTracker& operator=(const PerformanceTracker&) = delete;
  PerformanceTracker(PerformanceTracker&&) = delete;
  PerformanceTracker& operator=(PerformanceTracker&&) = delete;

  // Functions for recording performance information
  void Reset();

  /**
   * custom_value can be used if you are recording something with it's own DT. For example,
   * if you are recording the fallback of the throttler or the latency of the frame.
   *
   * If a custom_value is not supplied, the value will be set to the time between calls aka,
   * duration. This is the most common use case of this class, as an FPS counter.
   *
   * The boolean value_is_duration should be set to true if the custom DTs you are providing
   * represent a continuous duration. For example, the present times from a render backend
   * would set value_is_duration to true. Things like throttler fallback or frame latency
   * are not continuous, so they should not represent duration.
   */
  void Count(std::optional<DT> custom_value = std::nullopt, bool value_is_duration = false);

  // Functions for reading performance information
  DT GetSampleWindow() const;

  double GetHzAvg() const;

  DT GetDtAvg() const;
  DT GetDtStd() const;

  DT GetLastRawDt() const;

  void ImPlotPlotLines(const char* label) const;

private:  // Functions for managing dt queue
  inline void QueueClear();
  inline void QueuePush(TimeDataPair dt);
  inline const TimeDataPair& QueuePop();
  inline const TimeDataPair& QueueTop() const;
  inline const TimeDataPair& QueueBottom() const;

  inline std::size_t QueueSize() const;
  inline bool QueueEmpty() const;

  // Handle pausing and logging
  void LogRenderTimeToFile(DT val);
  void SetPaused(bool paused);

  bool m_paused = false;
  int m_on_state_changed_handle;

  // Name of log file and file stream
  std::optional<std::string> m_log_name;
  std::ofstream m_bench_file;

  // Last time Count() was called
  TimePoint m_last_time;

  // Amount of time to sample dt's over (defaults to config)
  const std::optional<s64> m_sample_window_us;

  // Queue + Running Total used to calculate average dt
  TimeDataPair m_dt_total;
  std::array<TimeDataPair, MAX_DT_QUEUE_SIZE> m_dt_queue;
  std::size_t m_dt_queue_begin = 0;
  std::size_t m_dt_queue_end = 0;

  // Average rate/time throughout the window
  DT m_dt_avg = DT::zero();  // Uses Moving Average
  double m_hz_avg = 0.0;     // Uses Moving Average + Euler Average

  // Used to initialize this on demand instead of on every Count()
  mutable std::optional<DT> m_dt_std = std::nullopt;

  // Used to enable thread safety with the performance tracker
  mutable std::shared_mutex m_mutex;
};
