// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Logging/LogManager.h"

class ConsoleListener : public Common::Log::LogListener
{
public:
  ConsoleListener();
  ~ConsoleListener();

  void Log(Common::Log::LogLevel level, const char* text) override;

private:
#if !defined _WIN32 && !defined ANDROID
  bool m_use_color = false;
#endif
};
