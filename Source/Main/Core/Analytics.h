// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/Analytics.h"
#include "Common/CommonTypes.h"

#if defined(ANDROID)
#include <functional>
#endif
// Non generic part of the Dolphin Analytics framework. See Common/Analytics.h
// for the main documentation.

class DolphinAnalytics
{
public:
  // Performs lazy-initialization of a singleton and returns the instance.
  static std::shared_ptr<DolphinAnalytics> Instance();

#if defined(ANDROID)
  // Get value from java.
  static void AndroidSetGetValFunc(std::function<std::string(std::string)> function);
#endif
  // Resets and recreates the analytics system in order to reload
  // configuration.
  void ReloadConfig();
  // Rotates the unique identifier used for this instance of Dolphin and saves
  // it into the configuration.
  void GenerateNewIdentity();

  // Reports a Dolphin start event.
  void ReportDolphinStart(const std::string& ui_type);

  // Generates a base report for a "Game start" event. Also preseeds the
  // per-game base data.
  void ReportGameStart();

  struct PerformanceSample
  {
    double speed_ratio;  // See SystemTimers::GetEstimatedEmulationPerformance().
    int num_prims;
    int num_draw_calls;
  };
  // Reports performance information. This method performs its own throttling / aggregation --
  // calling it does not guarantee when a report will actually be sent.
  //
  // This method is NOT thread-safe.
  void ReportPerformanceInfo(PerformanceSample&& sample);

  // Forward Send method calls to the reporter.
  template <typename T>
  void Send(T report)
  {
    std::lock_guard<std::mutex> lk(m_reporter_mutex);
    m_reporter.Send(report);
  }

private:
  DolphinAnalytics();

  void MakeBaseBuilder();
  void MakePerGameBuilder();

  // Returns a unique ID derived on the global unique ID, hashed with some
  // report-specific data. This avoid correlation between different types of
  // events.
  std::string MakeUniqueId(const std::string& data);

  // Unique ID. This should never leave the application. Only used derived
  // values created by MakeUniqueId.
  std::string m_unique_id;

  // Performance sampling configuration constants.
  //
  // 5min after startup + rand(0, 3min) jitter time, collect performance for 100 frames in a row.
  // Repeat collection after 30min + rand(0, 3min).
  static constexpr int NUM_PERFORMANCE_SAMPLES_PER_REPORT = 100;
  static constexpr int PERFORMANCE_SAMPLING_INITIAL_WAIT_TIME_SECS = 300;
  static constexpr int PERFORMANCE_SAMPLING_WAIT_TIME_JITTER_SECS = 180;
  static constexpr int PERFORMANCE_SAMPLING_INTERVAL_SECS = 1800;

  // Performance sampling state & internal helpers.
  void InitializePerformanceSampling();  // Called on game start / title switch.
  bool ShouldStartPerformanceSampling();
  u64 m_sampling_next_start_us;              // Next timestamp (in us) at which to trigger sampling.
  bool m_sampling_performance_info = false;  // Whether we are currently collecting samples.
  std::vector<PerformanceSample> m_performance_samples;

  // Builder that contains all non variable data that should be sent with all
  // reports.
  Common::AnalyticsReportBuilder m_base_builder;

  // Builder that contains per game data and is initialized when a game start
  // report is sent.
  Common::AnalyticsReportBuilder m_per_game_builder;

  std::mutex m_reporter_mutex;
  Common::AnalyticsReporter m_reporter;

  // Shared pointer in order to allow for multithreaded use of the instance and
  // avoid races at reinitialization time.
  static std::mutex s_instance_mutex;
  static std::shared_ptr<DolphinAnalytics> s_instance;
};
