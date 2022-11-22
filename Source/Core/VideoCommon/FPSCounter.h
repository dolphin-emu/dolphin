// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <fstream>

#include "Common/CommonTypes.h"

class FPSCounter
{
public:
  explicit FPSCounter(const char* log_name = "log.txt");
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
  void LogRenderTimeToFile(s64 val);
  void SetPaused(bool paused);

  const char* m_log_name;
  std::ofstream m_bench_file;

  bool m_paused = false;
  s64 m_last_time = 0;

  double m_avg_fps = 0.0;
  double m_raw_dt = 0.0;

  s64 m_dt_total = 0;
  std::deque<s64> m_dt_queue;

  int m_on_state_changed_handle = -1;
  s64 m_last_time_pause = 0;
};
