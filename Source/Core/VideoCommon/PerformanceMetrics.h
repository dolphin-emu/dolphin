// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/System.h"
#include "VideoCommon/PerformanceTracker.h"

class PerformanceMetrics
{
public:
  PerformanceMetrics() = default;
  ~PerformanceMetrics() = default;

  PerformanceMetrics(const PerformanceMetrics&) = delete;
  PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
  PerformanceMetrics(PerformanceMetrics&&) = delete;
  PerformanceMetrics& operator=(PerformanceMetrics&&) = delete;

public:  // Count Functions
  void Reset();
  void CountFrame();
  void CountVBlank();

  void CountThrottleSleep(DT sleep);
  void CountPerformanceMarker(Core::System& system, s64 cyclesLate);

public:  // Getter Functions
  double GetFPS() const;
  double GetVPS() const;
  double GetSpeed() const;

  double GetLastSpeedDenominator() const;

  double GetEstimatedEmulationSpeed() const;

public:  // ImGui Functions
  void DrawImGuiStats(const float backbuffer_scale) const;

private:  // Member Variables
  PerformanceTracker m_fps_counter{"render_times.txt"};
  PerformanceTracker m_vps_counter{"vblank_times.txt"};
  PerformanceTracker m_speed_counter{nullptr, 500000};

  double m_emulation_speed = 0;
  TimePoint m_real_time, m_cpu_time;
  DT m_real_dt, m_cpu_dt;
};

extern PerformanceMetrics g_perf_metrics;
