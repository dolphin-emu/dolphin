// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Thread.h"

#if USE_EGL
#include "DolphinWX/GLInterface/EGL.h"
#elif defined(__APPLE__)
#include "DolphinWX/GLInterface/AGL.h"
#elif defined(_WIN32)
#include "DolphinWX/GLInterface/WGL.h"
#elif HAVE_X11
#include "DolphinWX/GLInterface/GLX.h"
#include <GL/glx.h>
#else
#error Platform doesnt have a GLInterface
#endif

typedef struct {
#if USE_EGL
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLDisplay egl_dpy;
	EGLNativeWindowType native_window;
#elif defined(__APPLE__)
	NSView *cocoaWin;
	NSOpenGLContext *cocoaCtx;
#elif HAVE_X11
	GLXContext ctx;
	int screen;
	Window win;
	Window parent;
	// dpy used for glx stuff, evdpy for window events etc.
	// evdpy is to be used by XEventThread only
	Display *dpy, *evdpy;
	XVisualInfo *vi;
	XSetWindowAttributes attr;
	std::thread xEventThread;
#endif
} GLWindow;

extern GLWindow GLWin;
