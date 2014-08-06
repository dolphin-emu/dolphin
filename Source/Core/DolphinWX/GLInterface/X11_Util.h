// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class cX11Window
{
private:
	void XEventThread();
public:
	void CreateXWindow(void);
	void DestroyXWindow(void);
};
