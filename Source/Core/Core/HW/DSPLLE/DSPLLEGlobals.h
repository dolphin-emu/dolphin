// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/AudioCommon.h"
#include "Common/Common.h"

// TODO: Get rid of this file.

#define PROFILE   0

#if PROFILE
	void ProfilerDump(u64 count);
	void ProfilerInit();
	void ProfilerAddDelta(int addr, int delta);
	void ProfilerStart();
#endif
