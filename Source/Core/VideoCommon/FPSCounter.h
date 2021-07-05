// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fstream>

#include "Common/CommonTypes.h"

class FPSCounter
{
public:
  FPSCounter();
  ~FPSCounter();
  FPSCounter(const FPSCounter&) = delete;
  FPSCounter& operator=(const FPSCounter&) = delete;
  FPSCounter(FPSCounter&&) = delete;
  FPSCounter& operator=(FPSCounter&&) = delete;

  // Called when a frame is rendered (updated every second).
  void Update();

  float GetFPS() const { return m_fps; }
  double GetDeltaTime() const { return m_time_diff_secs; }

private:
  void SetPaused(bool paused);

  u64 m_last_time = 0;
  u64 m_time_since_update = 0;
  u64 m_last_time_pause = 0;
  u32 m_frame_counter = 0;
  int m_on_state_changed_handle = -1;
  float m_fps = 0.f;
  std::ofstream m_bench_file;
  double m_time_diff_secs = 0.0;

  void LogRenderTimeToFile(u64 val);
};
