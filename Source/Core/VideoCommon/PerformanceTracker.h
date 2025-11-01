// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <deque>
#include <fstream>
#include <optional>

#include "Common/CommonTypes.h"
#include "Common/SPSCQueue.h"

class PerformanceTracker
{
public:
  explicit PerformanceTracker(std::optional<std::string> log_name = std::nullopt);
  ~PerformanceTracker() = default;

  PerformanceTracker(const PerformanceTracker&) = delete;
  PerformanceTracker& operator=(const PerformanceTracker&) = delete;
  PerformanceTracker(PerformanceTracker&&) = delete;
  PerformanceTracker& operator=(PerformanceTracker&&) = delete;

  void Reset();

  // Calls must come from the same thread.
  // UpdateStats is expected to be called regularly to empty the SPSC queue.
  void UpdateStats();
  void ImPlotPlotLines(const char* label) const;

  // May call from any thread, but not concurrently, not that you'd want to..
  void Count();

  // May call from any thread.
  static DT GetSampleWindow();
  double GetHzAvg() const;
  DT GetDtAvg() const;
  DT GetDtStd() const;
  DT GetLastRawDt() const;
  void InvalidateLastTime();

private:
  void LogRenderTimeToFile(DT val);

  void HandleRawDt(DT diff);
  void PushFront(DT value);
  void PopBack();

  // Name of log file and file stream
  std::optional<std::string> m_log_name;
  std::ofstream m_bench_file;

  // Last time Count() was called
  TimePoint m_last_time;
  std::atomic<bool> m_is_last_time_sane = false;

  // Push'd from Count()
  //  and Pop'd from UpdateStats()
  Common::SPSCQueue<DT> m_raw_dts;
  std::atomic<DT> m_last_raw_dt = DT::zero();

  // Queue + Running Total used to calculate average dt
  DT m_dt_total = DT::zero();
  std::deque<DT> m_dt_queue;

  // Average rate/time throughout the window
  std::atomic<DT> m_dt_avg = DT::zero();  // Uses Moving Average
  std::atomic<double> m_hz_avg = 0.0;     // Uses Moving Average + Euler Average
  std::atomic<DT> m_dt_std = DT::zero();
};
