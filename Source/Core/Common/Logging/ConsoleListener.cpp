// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#else
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "Common/Logging/Log.h"
#endif

#include "Common/Logging/ConsoleListener.h"

ConsoleListener::ConsoleListener()
{
#if !defined(_WIN32) && !defined(__ANDROID__)
  m_use_color = !!isatty(fileno(stdout));
#endif
}

ConsoleListener::~ConsoleListener()
{
#if !defined(_WIN32) && !defined(__ANDROID__)
  fflush(nullptr);
#endif
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
#ifdef _WIN32
  ::OutputDebugStringA(text);
#elif defined(__ANDROID__)
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
#else
  char color_attr[16] = "";
  char reset_attr[16] = "";

  if (m_use_color)
  {
    strcpy(reset_attr, "\x1b[0m");
    switch (level)
    {
    case LogTypes::LOG_LEVELS::LNOTICE:
      // light green
      strcpy(color_attr, "\x1b[92m");
      break;
    case LogTypes::LOG_LEVELS::LERROR:
      // light red
      strcpy(color_attr, "\x1b[91m");
      break;
    case LogTypes::LOG_LEVELS::LWARNING:
      // light yellow
      strcpy(color_attr, "\x1b[93m");
      break;
    default:
      break;
    }
  }
  fprintf(stderr, "%s%s%s", color_attr, text, reset_attr);
#endif
}
