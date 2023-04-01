// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <fstream>

#include "Common/Timer.h"

class FPSCounter
{
public:
	// Initializes the FPS counter.
	FPSCounter();

	// Called when a frame is rendered (updated every second).
	void Update();

	unsigned int GetFPS() const { return m_fps; }

private:
	unsigned int m_fps = 0;
	unsigned int m_counter = 0;
	unsigned int m_fps_last_counter = 0;
	Common::Timer m_update_time;

	Common::Timer m_render_time;
	std::ofstream m_bench_file;

	void LogRenderTimeToFile(u64 val);
};
