// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

class Platform
{
public:
  virtual void Init() = 0;
  virtual void SetTitle(const std::string& title) = 0;
  virtual void MainLoop() = 0;
  virtual void Shutdown() = 0;
};
