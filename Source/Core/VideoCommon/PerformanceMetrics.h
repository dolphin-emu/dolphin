// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

  // Count Functions
  void Reset();
  void CountFrame();
  void CountVBlank();

  // Getter Functions
  double GetFPS() const;
  double GetVPS() const;
  double GetSpeed() const;

  double GetLastSpeedDenominator() const;

  // ImGui Functions
  void DrawImGuiStats(const float backbuffer_scale) const;

private:
  PerformanceTracker m_fps_counter{"render_times.txt"};
  PerformanceTracker m_vps_counter{"vblank_times.txt"};
  PerformanceTracker m_speed_counter{nullptr, 500000};
};

extern PerformanceMetrics g_perf_metrics;
