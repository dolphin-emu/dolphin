// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/VideoConfig.h"
#include <memory>

std::unique_ptr<PerfQueryBase> g_perf_query;

bool PerfQueryBase::ShouldEmulate() {
  return g_ActiveConfig.bPerfQueriesEnable;
}
