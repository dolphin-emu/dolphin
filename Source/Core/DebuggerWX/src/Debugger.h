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

#ifndef _DEBUGGER_H
#define _DEBUGGER_H

enum
{
	IDM_LOG,
	IDM_UPDATELOG,
	IDM_CLEARLOG,
	IDM_LOGCHECKS,
	IDM_ENABLEALL,
	IDM_SUBMITCMD = 300,
};

#define wxUSE_XPM_IN_MSW 1
#define USE_XPM_BITMAPS 1

#include "wx/wx.h"

// define this to use XPMs everywhere (by default, BMPs are used under Win)
// BMPs use less space, but aren't compiled into the executable on other platforms

#if USE_XPM_BITMAPS && defined (__WXMSW__) && !wxUSE_XPM_IN_MSW
#error You need to enable XPM support to use XPM bitmaps with toolbar!
#endif // USE_XPM_BITMAPS

#endif
