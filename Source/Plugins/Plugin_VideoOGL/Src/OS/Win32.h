// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _WIN32_H_
#define _WIN32_H_

#pragma once

#include "stdafx.h"

namespace EmuWindow
{
    extern int g_winstyle;

    HWND GetWnd();
    HWND GetParentWnd();
	HWND GetChildParentWnd();
    HWND Create(HWND hParent, HINSTANCE hInstance, const TCHAR *title);
    void Show();
    void Close();
    void SetSize(int displayWidth, int displayHeight);
}

#endif // _WIN32_H_
