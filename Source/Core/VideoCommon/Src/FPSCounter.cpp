// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "FPSCounter.h"
#include "FileUtil.h"
#include "Timer.h"
#include "VideoConfig.h"

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
	snprintf(buffer, 256, "%ld\n", val);
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