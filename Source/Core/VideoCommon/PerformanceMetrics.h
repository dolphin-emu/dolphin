// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <shared_mutex>

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

  void CountAudioLatency(DT latency);
  void CountThrottleSleep(DT sleep);
  void CountPerformanceMarker(Core::System& system, s64 cyclesLate);

  // Getter Functions
  double GetFPS() const;
  double GetVPS() const;
  double GetSpeed() const;
  double GetAudioSpeed() const;
  double GetMaxSpeed() const;

  double GetLastSpeedDenominator() const;

  // ImGui Functions
  void DrawImGuiStats(const float backbuffer_scale);

private:
  PerformanceTracker m_fps_counter{"render_times.txt"};
  PerformanceTracker m_vps_counter{"vblank_times.txt"};

  PerformanceTracker m_audio_latency_counter{};
  PerformanceTracker m_audio_speed_counter{std::nullopt, 128000};

  PerformanceTracker m_speed_counter{std::nullopt, 1280000};
  PerformanceTracker m_max_speed_counter{std::nullopt, 1280000};

  double m_graph_max_time = 0.0;

  mutable std::shared_mutex m_time_lock;
  TimePoint m_prev_adjusted_time{};
  DT m_time_sleeping{};
};

extern PerformanceMetrics g_perf_metrics;
