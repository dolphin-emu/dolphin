// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once

#include <functional>
#include <string>

#include "Common/Analytics.h"

namespace Common
{
void AndroidSetReportHandler(std::function<void(std::string, std::string)> function);

class AndroidAnalyticsBackend : public AnalyticsReportingBackend
{
public:
  explicit AndroidAnalyticsBackend(const std::string endpoint);

  ~AndroidAnalyticsBackend() override;
  void Send(std::string report) override;

private:
  std::string m_endpoint;
};
}  // namespace Common
