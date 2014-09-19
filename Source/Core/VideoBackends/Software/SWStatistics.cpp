// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstring>
#include "VideoBackends/Software/SWStatistics.h"

SWStatistics swstats;

SWStatistics::SWStatistics()
{
	frameCount = 0;
}

void SWStatistics::ResetFrame()
{
	memset(&thisFrame, 0, sizeof(ThisFrame));
}
