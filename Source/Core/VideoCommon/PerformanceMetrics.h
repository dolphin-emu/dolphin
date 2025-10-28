// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <deque>

#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "VideoCommon/PerformanceTracker.h"

namespace Core
{
class System;
}

class PerformanceMetrics
{
public:
  PerformanceMetrics() = default;
  ~PerformanceMetrics() = default;

  PerformanceMetrics(const PerformanceMetrics&) = delete;
  PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
  PerformanceMetrics(PerformanceMetrics&&) = delete;
  PerformanceMetrics& operator=(PerformanceMetrics&&) = delete;

  void Reset();

  void CountFrame();
  void CountVBlank();
  void OnEmulationStateChanged(Core::State state);

  // Call from CPU thread.
  void CountThrottleSleep(DT sleep);
  void AdjustClockSpeed(s64 ticks, u32 new_ppc_clock, u32 old_ppc_clock);
  void CountPerformanceMarker(s64 ticks, u32 ticks_per_second);

  // Getter Functions. May be called from any thread.
  double GetFPS() const;
  double GetVPS() const;
  double GetSpeed() const;
  double GetMaxSpeed() const;

  // Call from any thread.
  void SetLatestFramePresentationOffset(DT offset);

  // ImGui Functions
  void DrawImGuiStats(const float backbuffer_scale);

private:
  PerformanceTracker m_fps_counter{"render_times.txt"};
  PerformanceTracker m_vps_counter{"vblank_times.txt"};

  double m_graph_max_time = 0.0;

  std::atomic<double> m_speed{};
  std::atomic<double> m_max_speed{};

  std::atomic<DT> m_frame_presentation_offset{};

  struct PerfSample
  {
    TimePoint clock_time;
    TimePoint work_time;
    s64 core_ticks;
  };

  std::deque<PerfSample> m_samples;
  DT m_time_sleeping{};
};

extern PerformanceMetrics g_perf_metrics;
