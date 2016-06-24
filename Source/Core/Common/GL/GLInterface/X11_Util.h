// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <string>
#include <thread>

class cX11Window
{
private:
  void XEventThread();
  std::thread xEventThread;
  Colormap colormap;

public:
  void Initialize(Display* dpy);
  Window CreateXWindow(Window parent, XVisualInfo* vi);
  void DestroyXWindow();

  Display* dpy;
  Window win;
};
