// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Logging/ConsoleListener.h"

#include <android/log.h>

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(Common::Log::LogLevel level, const char* text)
{
  android_LogPriority logLevel = ANDROID_LOG_UNKNOWN;

  // Map dolphin's log levels to android's
  switch (level)
  {
  case Common::Log::LogLevel::LDEBUG:
    logLevel = ANDROID_LOG_DEBUG;
    break;
  case Common::Log::LogLevel::LINFO:
    logLevel = ANDROID_LOG_INFO;
    break;
  case Common::Log::LogLevel::LWARNING:
    logLevel = ANDROID_LOG_WARN;
    break;
  case Common::Log::LogLevel::LERROR:
    logLevel = ANDROID_LOG_ERROR;
    break;
  case Common::Log::LogLevel::LNOTICE:
    logLevel = ANDROID_LOG_INFO;
    break;
  }

  __android_log_write(logLevel, "Dolphinemu", text);
}
