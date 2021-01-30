// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/PerformanceSample.h"

class PerformanceSampleAggregator
{
public:
  PerformanceSampleAggregator() = default;

  static std::chrono::microseconds GetInitialSamplingStartTimestamp();
  static std::chrono::microseconds GetRepeatSamplingStartTimestamp();
  static std::chrono::microseconds GetCurrentMicroseconds();

  struct CompletedReport
  {
    std::vector<u32> speed;
    std::vector<u32> primitives;
    std::vector<u32> draw_calls;
  };

  static CompletedReport GetReportFromSamples(const std::vector<PerformanceSample>& samples);
};
