// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  ::OutputDebugStringW(UTF8ToUTF16(text).c_str());
}
