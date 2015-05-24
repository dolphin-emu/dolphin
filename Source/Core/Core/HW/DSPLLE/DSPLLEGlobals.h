// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// TODO: Get rid of this file.

#define PROFILE   0

#if PROFILE
	void ProfilerDump(u64 _count);
	void ProfilerInit();
	void ProfilerAddDelta(int _addr, int _delta);
	void ProfilerStart();
#endif
