// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Timer.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/VideoConfig.h"

static constexpr u64 FPS_REFRESH_INTERVAL = 1000;

FPSCounter::FPSCounter()
{
  m_update_time.Update();
  m_render_time.Update();
}

void FPSCounter::LogRenderTimeToFile(u64 val)
{
  if (!m_bench_file.is_open())
    m_bench_file.open(File::GetUserPath(D_LOGS_IDX) + "render_time.txt");

  m_bench_file << val << std::endl;
}

void FPSCounter::Update()
{
  if (m_update_time.GetTimeDifference() >= FPS_REFRESH_INTERVAL)
  {
    m_update_time.Update();
    m_fps = m_counter - m_fps_last_counter;
    m_fps_last_counter = m_counter;
    m_bench_file.flush();
  }

  if (g_ActiveConfig.bLogRenderTimeToFile)
  {
    LogRenderTimeToFile(m_render_time.GetTimeDifference());
    m_render_time.Update();
  }

  m_counter++;
}
