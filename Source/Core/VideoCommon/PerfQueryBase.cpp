// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/VideoConfig.h"

PerfQueryBase* g_perf_query = nullptr;

bool PerfQueryBase::ShouldEmulate()
{
	return g_ActiveConfig.bPerfQueriesEnable;
}
