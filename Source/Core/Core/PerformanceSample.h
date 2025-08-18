// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct PerformanceSample
{
  double speed_ratio;  // See SystemTimers::GetEstimatedEmulationPerformance().
  int num_prims;
  int num_draw_calls;
};
