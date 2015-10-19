// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLHaiku.h"

#include <Window.h>

EGLDisplay cInterfaceEGLHaiku::OpenDisplay()
{
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLNativeWindowType cInterfaceEGLHaiku::InitializePlatform(EGLNativeWindowType host_window, EGLConfig config)
{
	EGLint format;
	eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &format);
	//ANativeWindow_setBuffersGeometry(host_window, 0, 0, format);

	BWindow* window = (BWindow*)host_window;
	const int width = window->Size().width;
	const int height = window->Size().height;
	GLInterface->SetBackBufferDimensions(width, height);

	return host_window;
}

void cInterfaceEGLHaiku::ShutdownPlatform()
{
}

