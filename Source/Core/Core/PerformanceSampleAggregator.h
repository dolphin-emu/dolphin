// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/PerformanceSample.h"

class PerformanceSampleAggregator
{
public:
  PerformanceSampleAggregator();

  struct CompletedReport
  {
    std::vector<u32> speed;
    std::vector<u32> primitives;
    std::vector<u32> draw_calls;
  };

  // Called on game start / title switch.
  void InitializePerformanceSampling();
  void AddSampleIfSamplingInProgress(PerformanceSample&& sample);
  std::optional<CompletedReport> PopReportIfComplete();

private:
  // Called after sampling report is completed
  void ResetPerformanceSampling();

  std::vector<PerformanceSample> m_samples;
  std::chrono::microseconds m_next_starting_timestamp;
};
