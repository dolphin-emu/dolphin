// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PerformanceSampleAggregator.h"

#include <limits>

#include "Common/Random.h"

namespace
{
constexpr size_t s_samples_per_report = 100;

std::chrono::microseconds GetCurrentMicroseconds()
{
  const std::chrono::high_resolution_clock::duration time_since_epoch =
      std::chrono::high_resolution_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(time_since_epoch);
}

std::chrono::microseconds GetSamplingStartTimeJitter()
{
  constexpr long long max_delay = std::chrono::microseconds(std::chrono::minutes(3)).count();
  return std::chrono::microseconds(Common::Random::GenerateValue<u64>() % max_delay);
}

std::chrono::microseconds
GetSamplingStartTimestampUsingBaseDelay(const std::chrono::microseconds base_delay)
{
  const std::chrono::microseconds now = GetCurrentMicroseconds();
  const std::chrono::microseconds jitter = GetSamplingStartTimeJitter();
  const std::chrono::microseconds sampling_start_timestamp = now + base_delay + jitter;
  return sampling_start_timestamp;
}

std::chrono::microseconds GetInitialSamplingStartTimestamp()
{
  constexpr std::chrono::microseconds base_initial_delay = std::chrono::minutes(5);
  return GetSamplingStartTimestampUsingBaseDelay(base_initial_delay);
}

std::chrono::microseconds GetRepeatSamplingStartTimestamp()
{
  constexpr std::chrono::microseconds base_repeat_delay = std::chrono::minutes(30);
  return GetSamplingStartTimestampUsingBaseDelay(base_repeat_delay);
}

PerformanceSampleAggregator::CompletedReport
GetCompletedReport(const std::vector<PerformanceSample>& samples)
{
  PerformanceSampleAggregator::CompletedReport report;
  const size_t num_samples = samples.size();
  report.speed.resize(num_samples);
  report.primitives.resize(num_samples);
  report.draw_calls.resize(num_samples);

  for (size_t i = 0; i < num_samples; ++i)
  {
    const PerformanceSample& sample = samples[i];
    report.speed[i] = static_cast<u32>(sample.speed_ratio * 1'000);
    report.primitives[i] = sample.num_prims;
    report.draw_calls[i] = sample.num_draw_calls;
  }
  return report;
}
}  // namespace

PerformanceSampleAggregator::PerformanceSampleAggregator()
    : m_samples(std::vector<PerformanceSample>(s_samples_per_report)),
      m_next_starting_timestamp(std::numeric_limits<std::chrono::microseconds>::max())
{
}

void PerformanceSampleAggregator::InitializePerformanceSampling()
{
  m_samples.clear();
  m_next_starting_timestamp = GetInitialSamplingStartTimestamp();
}

void PerformanceSampleAggregator::ResetPerformanceSampling()
{
  m_samples.clear();
  m_next_starting_timestamp = GetRepeatSamplingStartTimestamp();
}

void PerformanceSampleAggregator::AddSampleIfSamplingInProgress(PerformanceSample&& sample)
{
  if (GetCurrentMicroseconds() >= m_next_starting_timestamp)
  {
    m_samples.push_back(sample);
  }
}

std::optional<PerformanceSampleAggregator::CompletedReport>
PerformanceSampleAggregator::PopReportIfComplete()
{
  if (m_samples.size() < s_samples_per_report)
  {
    return std::nullopt;
  }
  const CompletedReport report = GetCompletedReport(m_samples);
  ResetPerformanceSampling();
  return report;
}
