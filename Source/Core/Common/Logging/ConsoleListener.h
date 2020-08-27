// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Logging/LogManager.h"

class ConsoleListener : public Common::Log::LogListener
{
public:
  ConsoleListener();
  ~ConsoleListener();

  void Log(Common::Log::LOG_LEVELS level, const char* text) override;

private:
  bool m_use_color;
};
