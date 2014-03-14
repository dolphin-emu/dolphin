// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if USE_EGL
class cXInterface
{
private:
	void XEventThread();
public:
	bool ServerConnect(void);
	bool Initialize(void *config, void *window_handle);
	void *EGLGetDisplay(void);
	void *CreateWindow(void);
	void DestroyWindow(void);
	void UpdateFPSDisplay(const std::string& text);
	void SwapBuffers();
};
#else
class cX11Window
{
private:
	void XEventThread();
public:
	void CreateXWindow(void);
	void DestroyXWindow(void);
};
#endif
