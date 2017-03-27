// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Logging/LogManager.h"

class ConsoleListener : public LogListener
{
public:
  ConsoleListener();
  ~ConsoleListener();

  void Log(LogTypes::LOG_LEVELS, const char* text) override;

private:
#if !defined(__ANDROID__) && !defined(_WIN32)
  bool m_use_color;
#endif
};
