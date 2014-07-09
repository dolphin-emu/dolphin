// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <fstream>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/VideoConfig.h"

namespace FPSCounter
{
#define FPS_REFRESH_INTERVAL 1000

static unsigned int s_counter = 0;
static unsigned int s_fps = 0;
static unsigned int s_fps_last_counter = 0;
static unsigned long s_last_update_time = 0;
static std::ofstream s_bench_file;

static Common::Timer s_render_time;
static std::ofstream s_time_file;

void Initialize()
{
	s_counter = s_fps_last_counter = 0;
	s_fps = 0;
	s_last_update_time = Common::Timer::GetTimeMs();

	s_render_time.Update();

	if (s_bench_file.is_open())
		s_bench_file.close();
	if (s_time_file.is_open())
		s_time_file.close();
}

static void LogFPSToFile(u64 val)
{
	if (!s_bench_file.is_open())
		s_bench_file.open(File::GetUserPath(D_LOGS_IDX) + "fps.txt");

	s_bench_file << StringFromFormat("%lu\n", val);
}

static void LogRenderTimeToFile(u64 val)
{
	if (!s_time_file.is_open())
		s_time_file.open(File::GetUserPath(D_LOGS_IDX) + "render_time.txt");

	s_time_file << StringFromFormat("%lu\n", val);
}

int Update()
{
	if (Common::Timer::GetTimeMs() - s_last_update_time >= FPS_REFRESH_INTERVAL)
	{
		s_last_update_time = Common::Timer::GetTimeMs();
		s_fps = s_counter - s_fps_last_counter;
		s_fps_last_counter = s_counter;
		if (g_ActiveConfig.bLogFPSToFile)
			LogFPSToFile(s_fps);
	}

	if (g_ActiveConfig.bLogRenderTimeToFile)
	{
		LogRenderTimeToFile(s_render_time.GetTimeDifference());
		s_render_time.Update();
	}

	s_counter++;
	return s_fps;
}
}