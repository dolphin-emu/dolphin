// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Logging/ConsoleListener.h"

#include <cstdio>
#include <string_view>

#include <stdio.h>  // for f[un]lockfile
#include <unistd.h>

#include "Common/Logging/Log.h"

ConsoleListener::ConsoleListener() : m_use_color{isatty(STDERR_FILENO) == 1}
{
}

ConsoleListener::~ConsoleListener()
{
  std::fflush(nullptr);
}

void ConsoleListener::Log(Common::Log::LogLevel level, const char* text)
{
  std::string_view color_attr;

  if (m_use_color)
  {
    switch (level)
    {
    case Common::Log::LogLevel::LNOTICE:
      // light green
      color_attr = "\x1b[92m";
      break;
    case Common::Log::LogLevel::LERROR:
      // light red
      color_attr = "\x1b[91m";
      break;
    case Common::Log::LogLevel::LWARNING:
      // light yellow
      color_attr = "\x1b[93m";
      break;
    case Common::Log::LogLevel::LINFO:
      // light cyan
      color_attr = "\x1b[96m";
      break;
    default:
      break;
    }

    if (!color_attr.empty())
    {
      flockfile(stderr);
      std::fwrite(color_attr.data(), 1, color_attr.size(), stderr);
      std::fputs(text, stderr);
      std::fputs("\x1b[0m", stderr);
      funlockfile(stderr);
      return;
    }
  }

  std::fputs(text, stderr);
}
