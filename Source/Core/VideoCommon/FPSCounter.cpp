// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/Timer.h"

#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/VideoConfig.h"

#define FPS_REFRESH_INTERVAL 1000

static unsigned int s_counter = 0;
static unsigned int s_fps = 0;
static unsigned int s_fps_last_counter = 0;
static unsigned long s_last_update_time = 0;
static File::IOFile s_bench_file;

void InitFPSCounter()
{
	s_counter = s_fps_last_counter = 0;
	s_fps = 0;
	s_last_update_time = Common::Timer::GetTimeMs();

	if (s_bench_file.IsOpen())
		s_bench_file.Close();
}

static void LogFPSToFile(unsigned long val)
{
	if (!s_bench_file.IsOpen())
		s_bench_file.Open(File::GetUserPath(D_LOGS_IDX) + "fps.txt", "w");

	char buffer[256];
	snprintf(buffer, 256, "%lu\n", val);
	s_bench_file.WriteArray(buffer, strlen(buffer));
}

int UpdateFPSCounter()
{
	if (Common::Timer::GetTimeMs() - s_last_update_time >= FPS_REFRESH_INTERVAL)
	{
		s_last_update_time = Common::Timer::GetTimeMs();
		s_fps = s_counter - s_fps_last_counter;
		s_fps_last_counter = s_counter;
		if (g_ActiveConfig.bLogFPSToFile)
			LogFPSToFile(s_fps);
	}

	s_counter++;
	return s_fps;
}