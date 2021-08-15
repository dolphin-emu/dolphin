// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "Common/Logging/ConsoleListener.h"
#include "Common/StringUtil.h"

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log([[maybe_unused]] Common::Log::LOG_LEVELS level, const char* text)
{
  ::OutputDebugStringW(UTF8ToWString(text).c_str());
}
