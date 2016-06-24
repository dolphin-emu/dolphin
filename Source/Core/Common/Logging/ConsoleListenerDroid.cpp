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

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  android_LogPriority logLevel = ANDROID_LOG_UNKNOWN;

  // Map dolphin's log levels to android's
  switch (level)
  {
  case LogTypes::LOG_LEVELS::LDEBUG:
    logLevel = ANDROID_LOG_DEBUG;
    break;
  case LogTypes::LOG_LEVELS::LINFO:
    logLevel = ANDROID_LOG_INFO;
    break;
  case LogTypes::LOG_LEVELS::LWARNING:
    logLevel = ANDROID_LOG_WARN;
    break;
  case LogTypes::LOG_LEVELS::LERROR:
    logLevel = ANDROID_LOG_ERROR;
    break;
  case LogTypes::LOG_LEVELS::LNOTICE:
    logLevel = ANDROID_LOG_INFO;
    break;
  }

  __android_log_write(logLevel, "Dolphinemu", text);
}
