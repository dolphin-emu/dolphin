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

static constexpr double US_TO_MS = 1000.0;
static constexpr double US_TO_S = 1000000.0;

static constexpr s64 FPS_SAMPLE_TIME_US = 2000000;

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

void FPSCounter::LogRenderTimeToFile(s64 val)
{
  if (!m_bench_file.is_open())
  {
    File::OpenFStream(m_bench_file, File::GetUserPath(D_LOGS_IDX) + m_log_name, std::ios_base::out);
  }

  m_bench_file << std::fixed << std::setprecision(8) << (val / US_TO_MS) << std::endl;
}

void FPSCounter::Update()
{
  if (m_paused) return;

  const s64 time = Common::Timer::NowUs();
  const s64 diff = std::max<s64>(0, time - m_last_time);
  m_last_time = time;

  m_dt_total += diff;
  m_dt_queue.push(diff);

  while (FPS_SAMPLE_TIME_US <= m_dt_total - m_dt_queue.front())
  {
    m_dt_total -= m_dt_queue.front();
    m_dt_queue.pop();
  }

  m_avg_fps = (US_TO_S * m_dt_queue.size()) / m_dt_total;
  m_raw_dt = diff / US_TO_S;

  if (g_ActiveConfig.bLogRenderTimeToFile)
    LogRenderTimeToFile(diff);
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
