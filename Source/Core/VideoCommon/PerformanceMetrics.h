// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>

#include "Common/CommonTypes.h"
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

  // Count Functions
  void Reset();
  void CountFrame();
  void CountVBlank();

  void CountThrottleSleep(DT sleep);
  void CountPerformanceMarker(Core::System& system, s64 cyclesLate);

  // Getter Functions
  double GetFPS() const;
  double GetVPS() const;
  double GetSpeed() const;
  double GetMaxSpeed() const;

  double GetLastSpeedDenominator() const;

  // ImGui Functions
  void DrawImGuiStats(const float backbuffer_scale);

private:
  PerformanceTracker m_fps_counter{"render_times.txt"};
  PerformanceTracker m_vps_counter{"vblank_times.txt"};
  PerformanceTracker m_speed_counter{std::nullopt, 1000000};

  double m_graph_max_time = 0.0;

  std::atomic<double> m_max_speed{};
  u8 m_time_index = 0;
  std::array<TimePoint, 256> m_real_times{};
  std::array<u64, 256> m_core_ticks{};
  DT m_time_sleeping{};
};

extern PerformanceMetrics g_perf_metrics;
