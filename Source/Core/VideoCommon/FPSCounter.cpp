// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FPSCounter.h"

#include <fstream>
#include <iomanip>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "VideoCommon/VideoConfig.h"

static constexpr u64 FPS_REFRESH_INTERVAL_US = 250000;

FPSCounter::FPSCounter()
{
  m_last_time = Common::Timer::GetTimeUs();

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
    File::OpenFStream(m_bench_file, File::GetUserPath(D_LOGS_IDX) + "render_time.txt",
                      std::ios_base::out);
  }

  m_bench_file << std::fixed << std::setprecision(8) << (val / 1000.0) << std::endl;
}

void FPSCounter::Update()
{
  const u64 time = Common::Timer::GetTimeUs();
  const u64 diff = time - m_last_time;
  m_time_diff_secs = static_cast<double>(diff / 1000000.0);
  if (g_ActiveConfig.bLogRenderTimeToFile)
    LogRenderTimeToFile(diff);

  m_frame_counter++;
  m_time_since_update += diff;
  m_last_time = time;

  if (m_time_since_update >= FPS_REFRESH_INTERVAL_US)
  {
    m_fps = m_frame_counter / (m_time_since_update / 1000000.0);
    m_frame_counter = 0;
    m_time_since_update = 0;
  }
}

void FPSCounter::SetPaused(bool paused)
{
  if (paused)
  {
    m_last_time_pause = Common::Timer::GetTimeUs();
  }
  else
  {
    const u64 time = Common::Timer::GetTimeUs();
    const u64 diff = time - m_last_time_pause;
    m_last_time += diff;
  }
}
