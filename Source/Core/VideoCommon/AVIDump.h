// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <stdint.h>
#endif

#include "Common/CommonTypes.h"

class AVIDump
{
private:
	static bool CreateFile();
	static void CloseFile();

public:
	static bool Start(int w, int h);
	static void AddFrame(const u8* data, int width, int height);
	static void Stop();
	static void DoState();
};
