// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerfQueryBase.h"
#include <memory>
#include "VideoCommon/VideoConfig.h"

std::unique_ptr<PerfQueryBase> g_perf_query;

bool PerfQueryBase::ShouldEmulate()
{
  return g_ActiveConfig.bPerfQueriesEnable;
}
