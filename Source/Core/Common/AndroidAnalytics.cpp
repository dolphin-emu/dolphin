// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <functional>
#include <string>

#include "Common/AndroidAnalytics.h"

namespace Common
{
std::function<void(std::string, std::string)> s_android_send_report;
void AndroidSetReportHandler(std::function<void(std::string, std::string)> func)
{
  s_android_send_report = std::move(func);
}
AndroidAnalyticsBackend::AndroidAnalyticsBackend(std::string passed_endpoint)
    : m_endpoint{std::move(passed_endpoint)}
{
}
AndroidAnalyticsBackend::~AndroidAnalyticsBackend() = default;

void AndroidAnalyticsBackend::Send(std::string report)
{
  s_android_send_report(m_endpoint, report);
}
}  // namespace Common
