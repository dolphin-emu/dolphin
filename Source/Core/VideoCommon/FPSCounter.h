// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <fstream>

#include "Common/Timer.h"

class FPSCounter
{
public:
	unsigned int m_fps;

	// Initializes the FPS counter.
	FPSCounter();

	// Called when a frame is rendered. Returns the value to be displayed on
	// screen as the FPS counter (updated every second).
	int Update();

private:
	unsigned int m_counter;
	unsigned int m_fps_last_counter;
	Common::Timer m_update_time;

	Common::Timer m_render_time;
	std::ofstream m_bench_file;

	void LogRenderTimeToFile(u64 val);
};
