// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <string>
#include <thread>

#include "Platform.h"

class PlatformHeadless : final Platform
{
public:
  virtual void Init() override {}
  virtual void SetTitle(const std::string& title) override {}
  virtual void MainLoop() override
  {
    while (s_running.IsSet())
    {
      Core::HostDispatchJobs();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  virtual void Shutdown() override {}
};
