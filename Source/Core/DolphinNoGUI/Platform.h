// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

class Platform
{
public:
  virtual void Init() {}
  virtual void SetTitle(const std::string& title) {}
  virtual void MainLoop()
  {
    while (s_running.IsSet())
    {
      Core::HostDispatchJobs();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  virtual void Shutdown() {}
  virtual ~Platform() {}
};
