// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "Common.h"
#include "Config.h"

#include "VideoCommon.h"
#include "pluginspecs_video.h"

#include "ConsoleWindow.h"

// Turns file logging on and off
extern bool LocalLogFile;

// A global plugin specification
extern PLUGIN_GLOBALS* globals;

void OpenConsole();
void CloseConsole();

#endif // _GLOBALS_H
