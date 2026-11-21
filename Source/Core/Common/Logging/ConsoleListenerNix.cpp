// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Logging/ConsoleListener.h"

#include <cstdio>
#include <string_view>

#include <unistd.h>

#include <fmt/format.h>

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
      fmt::print(stderr, "{}{}\x1b[0m", color_attr, text);
      return;
    }
  }

  std::fputs(text, stderr);
}
