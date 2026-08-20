// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <deque>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"
#include "VideoCommon/PerformanceTracker.h"

namespace Core
{
class System;
}

class PerformanceMetrics
{
public:
  PerformanceMetrics();
  ~PerformanceMetrics() = default;

  PerformanceMetrics(const PerformanceMetrics&) = delete;
  PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
  PerformanceMetrics(PerformanceMetrics&&) = delete;
  PerformanceMetrics& operator=(PerformanceMetrics&&) = delete;

  void Reset();

  void CountFrame();
  void CountVBlank();

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
  void SetLatestFrameBufferSize(u32 width, u32 height);

  // ImGui Functions
  void DrawImGuiStats(const float backbuffer_scale);

private:
  struct FrameBufferSize
  {
    FrameBufferSize() : width(0), height(0) {}
    FrameBufferSize(u32 w, u32 h) : width(w), height(h) {}
    u32 width;
    u32 height;
  };

  PerformanceTracker m_fps_counter{"render_times.txt"};
  PerformanceTracker m_vps_counter{"vblank_times.txt"};

  double m_graph_max_time = 0.0;

  std::atomic<double> m_speed{};
  std::atomic<double> m_max_speed{};

  std::atomic<DT> m_frame_presentation_offset{};
  std::atomic<FrameBufferSize> m_frame_buffer_size{};

  struct PerfSample
  {
    TimePoint clock_time;
    TimePoint work_time;
    s64 core_ticks;
  };

  std::deque<PerfSample> m_samples;
  DT m_time_sleeping{};

  Common::EventHook m_state_change_hook;
};
