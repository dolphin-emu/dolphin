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

  double GetFPS() const { return m_avg_fps; }
  double GetDeltaTime() const { return m_raw_dt; }

private:
  void LogRenderTimeToFile(u64 val);
  void SetPaused(bool paused);

  std::ofstream m_bench_file;

  s64 m_last_time = 0;

  double m_avg_fps = 0.0;
  double m_raw_dt = 0.0;

  int m_on_state_changed_handle = -1;
  s64 m_last_time_pause = 0;
};
