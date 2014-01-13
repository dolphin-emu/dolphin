// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _GLINTERFACE_H_
#define _GLINTERFACE_H_

#if USE_EGL
#include "GLInterface/Platform.h"
#else

#include "Thread.h"
#if defined(USE_EGL) && USE_EGL
#include "GLInterface/EGL.h"
#elif defined(__APPLE__)
#include "GLInterface/AGL.h"
#elif defined(_WIN32)
#include "GLInterface/WGL.h"
#elif defined(HAVE_X11) && HAVE_X11
#include "GLInterface/GLX.h"
#include <GL/glx.h>
#else
#error Platform doesnt have a GLInterface
#endif


typedef struct {
#if defined(USE_EGL) && USE_EGL // This is currently a X11/EGL implementation for desktop
	int screen;
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLDisplay egl_dpy;
	int x, y;
	unsigned int width, height;
#elif defined(__APPLE__)
	NSView *cocoaWin;
	NSOpenGLContext *cocoaCtx;
#elif defined(HAVE_X11) && HAVE_X11
	int screen;
	Window win;
	Window parent;
	// dpy used for glx stuff, evdpy for window events etc.
	// evdpy is to be used by XEventThread only
	Display *dpy, *evdpy;
	XVisualInfo *vi;
	GLXContext ctx;
	XSetWindowAttributes attr;
	std::thread xEventThread;
	int x, y;
	unsigned int width, height;
#endif
} GLWindow;

enum GLInterfaceMode {
	MODE_OPENGL = 0,
	MODE_OPENGLES2,
	MODE_OPENGLES3,
};
extern cInterfaceBase *GLInterface;
extern GLWindow GLWin;

#endif
#endif
