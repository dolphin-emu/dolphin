// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <android/log.h>

#include "Common/Logging/ConsoleListener.h"

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(Common::Log::LOG_LEVELS level, const char* text)
{
  android_LogPriority logLevel = ANDROID_LOG_UNKNOWN;

  // Map dolphin's log levels to android's
  switch (level)
  {
  case Common::Log::LOG_LEVELS::LDEBUG:
    logLevel = ANDROID_LOG_DEBUG;
    break;
  case Common::Log::LOG_LEVELS::LINFO:
    logLevel = ANDROID_LOG_INFO;
    break;
  case Common::Log::LOG_LEVELS::LWARNING:
    logLevel = ANDROID_LOG_WARN;
    break;
  case Common::Log::LOG_LEVELS::LERROR:
    logLevel = ANDROID_LOG_ERROR;
    break;
  case Common::Log::LOG_LEVELS::LNOTICE:
    logLevel = ANDROID_LOG_INFO;
    break;
  }

  __android_log_write(logLevel, "Dolphinemu", text);
}
