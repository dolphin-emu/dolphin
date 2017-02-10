// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "Common/Analytics.h"

// Non generic part of the Dolphin Analytics framework. See Common/Analytics.h
// for the main documentation.

class DolphinAnalytics
{
public:
  // Performs lazy-initialization of a singleton and returns the instance.
  static std::shared_ptr<DolphinAnalytics> Instance();

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
