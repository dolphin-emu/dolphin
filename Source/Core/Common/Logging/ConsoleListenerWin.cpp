// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <windows.h>

#include "Common/Logging/ConsoleListener.h"

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  ::OutputDebugStringA(text);
}
