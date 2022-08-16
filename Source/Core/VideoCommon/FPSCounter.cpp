// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FPSCounter.h"

#include <cmath>
#include <fstream>
#include <iomanip>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "VideoCommon/VideoConfig.h"

static constexpr double FPS_COUNTER_RC = 0.8;

FPSCounter::FPSCounter(const char* log_name)
{
  m_last_time = Common::Timer::NowUs();
  m_log_name = log_name;

  m_on_state_changed_handle = Core::AddOnStateChangedCallback([this](Core::State state) {
    if (state == Core::State::Paused)
      SetPaused(true);
    else if (state == Core::State::Running)
      SetPaused(false);
  });
}

FPSCounter::~FPSCounter()
{
  Core::RemoveOnStateChangedCallback(&m_on_state_changed_handle);
}

void FPSCounter::LogRenderTimeToFile(u64 val)
{
  if (!m_bench_file.is_open())
  {
    File::OpenFStream(m_bench_file, File::GetUserPath(D_LOGS_IDX) + m_log_name, std::ios_base::out);
  }

  m_bench_file << std::fixed << std::setprecision(8) << (val / 1000.0) << std::endl;
}

void FPSCounter::Update()
{
  if (!m_paused)
  {
    const s64 time = Common::Timer::NowUs();
    const s64 diff = std::max<s64>(1, time - m_last_time);
    m_last_time = time;

    if (g_ActiveConfig.bLogRenderTimeToFile)
      LogRenderTimeToFile(diff);

    m_raw_dt = static_cast<double>(diff / 1000000.0);

    const double raw_fps = 1.0 / m_raw_dt;
    const double a = 1.0 - std::exp(-m_raw_dt / FPS_COUNTER_RC);
    m_avg_fps += a * (raw_fps - m_avg_fps);
  }
}

void FPSCounter::SetPaused(bool paused)
{
  m_paused = paused;
  if (m_paused)
  {
    m_last_time_pause = Common::Timer::NowUs();
  }
  else
  {
    const s64 time = Common::Timer::NowUs();
    const s64 diff = time - m_last_time_pause;
    m_last_time += diff;
  }
}
