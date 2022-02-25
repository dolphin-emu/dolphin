// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Logging/ConsoleListener.h"

#include <cstdio>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Common/Logging/Log.h"

ConsoleListener::ConsoleListener()
{
  m_use_color = !!isatty(fileno(stdout));
}

ConsoleListener::~ConsoleListener()
{
  fflush(nullptr);
}

void ConsoleListener::Log(Common::Log::LogLevel level, const char* text)
{
  char color_attr[16] = "";
  char reset_attr[16] = "";

  if (m_use_color)
  {
    strcpy(reset_attr, "\x1b[0m");
    switch (level)
    {
    case Common::Log::LogLevel::LNOTICE:
      // light green
      strcpy(color_attr, "\x1b[92m");
      break;
    case Common::Log::LogLevel::LERROR:
      // light red
      strcpy(color_attr, "\x1b[91m");
      break;
    case Common::Log::LogLevel::LWARNING:
      // light yellow
      strcpy(color_attr, "\x1b[93m");
      break;
    default:
      break;
    }
  }
  fprintf(stderr, "%s%s%s", color_attr, text, reset_attr);
}
