// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <thread>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class cX11Window
{
private:
	void XEventThread();
	std::thread xEventThread;
	Colormap colormap;
public:
	void Initialize(Display *dpy);
	Window CreateXWindow(Window parent, XVisualInfo *vi);
	void DestroyXWindow();

	Display *dpy;
	Window win;
};
