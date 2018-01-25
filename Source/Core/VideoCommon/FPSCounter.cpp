// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <iomanip>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Timer.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/VideoConfig.h"

static constexpr u64 FPS_REFRESH_INTERVAL = 250000;

FPSCounter::FPSCounter()
{
  m_last_time = Common::Timer::GetTimeUs();
}

void FPSCounter::LogRenderTimeToFile(u64 val)
{
  if (!m_bench_file.is_open())
    m_bench_file.open(File::GetUserPath(D_LOGS_IDX) + "render_time.txt");

  m_bench_file << std::fixed << std::setprecision(8) << (val / 1000.0) << std::endl;
}

void FPSCounter::Update()
{
  u64 time = Common::Timer::GetTimeUs();
  u64 diff = time - m_last_time;
  if (g_ActiveConfig.bLogRenderTimeToFile)
    LogRenderTimeToFile(diff);

  m_frame_counter++;
  m_time_since_update += diff;
  m_last_time = time;

  if (m_time_since_update >= FPS_REFRESH_INTERVAL)
  {
    m_fps = m_frame_counter / (m_time_since_update / 1000000.0);
    m_frame_counter = 0;
    m_time_since_update = 0;
  }
}
