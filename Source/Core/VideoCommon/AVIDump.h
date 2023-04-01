// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class AVIDump
{
private:
	static bool CreateFile();
	static void CloseFile();

public:
	enum class DumpFormat
	{
		FORMAT_BGR,
		FORMAT_RGBA
	};

	static bool Start(int w, int h, DumpFormat format);
	static void AddFrame(const u8* data, int width, int height);
	static void Stop();
	static void DoState();
};
