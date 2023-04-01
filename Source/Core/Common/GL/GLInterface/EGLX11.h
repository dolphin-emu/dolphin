// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <X11/Xlib.h>

#include "Common/GL/GLInterface/EGL.h"
#include "Common/GL/GLInterface/X11_Util.h"

class cInterfaceEGLX11 : public cInterfaceEGL
{
private:
	cX11Window XWindow;
	Display *dpy;
protected:
	EGLDisplay OpenDisplay() override;
	EGLNativeWindowType InitializePlatform(EGLNativeWindowType host_window, EGLConfig config) override;
	void ShutdownPlatform() override;
};
