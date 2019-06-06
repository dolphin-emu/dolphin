// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/HttpRequest.h"
#include "Common/SPSCQueue.h"

// Utilities for analytics reporting in Dolphin. This reporting is designed to
// provide anonymous data about how well Dolphin performs in the wild. It also
// allows developers to declare trace points in Dolphin's source code and get
// information about what games trigger these conditions.
//
// This unfortunately implements Yet Another Serialization Framework within
// Dolphin. We cannot really use ChunkFile because there is precedents for
// backwards incompatible changes in the ChunkFile format. We could use
// something like protobuf but setting up external dependencies is Hard™.
//
// Example usage:
//
// static auto s_reporter = std::make_unique<AnalyticsReporter>();
// if (user_gave_consent)
// {
//     s_reporter->SetBackend(std::make_unique<MyReportingBackend>());
// }
// s_reporter->Send(s_reporter->Builder()
//     .AddData("my_key", 42)
//     .AddData("other_key", false));

namespace Common
{
// Generic interface for an analytics reporting backends. The main
// implementation used in Dolphin can be found in Core/Analytics.h.
class AnalyticsReportingBackend
{
public:
  virtual ~AnalyticsReportingBackend() = default;
  // Called from the AnalyticsReporter backend thread.
  virtual void Send(std::string report) = 0;
};

// Builder object for an analytics report.
class AnalyticsReportBuilder
{
public:
  AnalyticsReportBuilder();
  ~AnalyticsReportBuilder() = default;

  AnalyticsReportBuilder(const AnalyticsReportBuilder& other) { *this = other; }
  AnalyticsReportBuilder(AnalyticsReportBuilder&& other)
  {
    std::lock_guard lk{other.m_lock};
    m_report = std::move(other.m_report);
  }

  const AnalyticsReportBuilder& operator=(const AnalyticsReportBuilder& other)
  {
    if (this != &other)
    {
      std::scoped_lock lk{m_lock, other.m_lock};
      m_report = other.m_report;
    }
    return *this;
  }

  // Append another builder to this one.
  AnalyticsReportBuilder& AddBuilder(const AnalyticsReportBuilder& other)
  {
    // Get before locking the object to avoid deadlocks with this += this.
    std::string other_report = other.Get();
    std::lock_guard lk{m_lock};
    m_report += other_report;
    return *this;
  }

  template <typename T>
  AnalyticsReportBuilder& AddData(std::string_view key, const T& value)
  {
    std::lock_guard lk{m_lock};
    AppendSerializedValue(&m_report, key);
    AppendSerializedValue(&m_report, value);
    return *this;
  }

  template <typename T>
  AnalyticsReportBuilder& AddData(std::string_view key, const std::vector<T>& value)
  {
    std::lock_guard lk{m_lock};
    AppendSerializedValue(&m_report, key);
    AppendSerializedValueVector(&m_report, value);
    return *this;
  }

  std::string Get() const
  {
    std::lock_guard lk{m_lock};
    return m_report;
  }

  // More efficient version of Get().
  std::string Consume()
  {
    std::lock_guard lk{m_lock};
    return std::move(m_report);
  }

protected:
  static void AppendSerializedValue(std::string* report, std::string_view v);
  static void AppendSerializedValue(std::string* report, const char* v);
  static void AppendSerializedValue(std::string* report, bool v);
  static void AppendSerializedValue(std::string* report, u64 v);
  static void AppendSerializedValue(std::string* report, s64 v);
  static void AppendSerializedValue(std::string* report, u32 v);
  static void AppendSerializedValue(std::string* report, s32 v);
  static void AppendSerializedValue(std::string* report, float v);

  static void AppendSerializedValueVector(std::string* report, const std::vector<u32>& v);

  // Should really be a std::shared_mutex, unfortunately that's C++17 only.
  mutable std::mutex m_lock;
  std::string m_report;
};

class AnalyticsReporter
{
public:
  AnalyticsReporter();
  ~AnalyticsReporter();

  // Sets a reporting backend and enables sending reports. Do not set a remote
  // backend without user consent.
  void SetBackend(std::unique_ptr<AnalyticsReportingBackend> backend)
  {
    m_backend = std::move(backend);
    m_reporter_event.Set();  // In case reports are waiting queued.
  }

  // Gets the base report builder which is closed for each subsequent report
  // being sent. DO NOT use this builder to send a report. Only use it to add
  // new fields that should be globally available.
  AnalyticsReportBuilder& BaseBuilder() { return m_base_builder; }
  // Gets a cloned builder that can be used to send a report.
  AnalyticsReportBuilder Builder() const { return m_base_builder; }
  // Enqueues a report for sending. Consumes the report builder.
  void Send(AnalyticsReportBuilder&& report);

  // For convenience.
  void Send(AnalyticsReportBuilder& report) { Send(std::move(report)); }

protected:
  void ThreadProc();

  std::shared_ptr<AnalyticsReportingBackend> m_backend;
  AnalyticsReportBuilder m_base_builder;

  std::thread m_reporter_thread;
  Common::Event m_reporter_event;
  Common::Flag m_reporter_stop_request;
  SPSCQueue<std::string> m_reports_queue;
};

// Analytics backend to be used for debugging purpose, which dumps reports to
// stdout.
class StdoutAnalyticsBackend : public AnalyticsReportingBackend
{
public:
  void Send(std::string report) override;
};

// Analytics backend that POSTs data to a remote HTTP(s) endpoint. WARNING:
// remember to get explicit user consent before using.
class HttpAnalyticsBackend : public AnalyticsReportingBackend
{
public:
  explicit HttpAnalyticsBackend(std::string endpoint);
  ~HttpAnalyticsBackend() override;

  void Send(std::string report) override;

protected:
  std::string m_endpoint;
  HttpRequest m_http{std::chrono::seconds{5}};
};
}  // namespace Common
