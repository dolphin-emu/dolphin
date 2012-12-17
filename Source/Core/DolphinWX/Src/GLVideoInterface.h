// Copyright (C) 2003 Dolphin Project.

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
#ifndef _VIDEOINTERFACE_H_
#define _VIDEOINTERFACE_H_

#include "Thread.h"

#if defined(USE_EGL) && USE_EGL
#include "VideoInterface/EGL.h"
#elif defined(USE_WX) && USE_WX
#include "VideoInterface/WX.h"
#elif defined(__APPLE__)
#include "VideoInterface/AGL.h"
#elif defined(_WIN32)
#include "VideoInterface/WGL.h"
#elif defined(HAVE_X11) && HAVE_X11
#include "VideoInterface/GLX.h"
#endif

typedef struct {
#if defined(USE_EGL) && USE_EGL // This is currently a X11/EGL implementation for desktop
	int screen;
	Display *x_dpy;
	Display *x_evdpy;
	Window win;
	Window parent;
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLDisplay egl_dpy;
	XVisualInfo *vi;
	XSetWindowAttributes attr;
	std::thread xEventThread;
	int x, y;
	unsigned int width, height;
#elif defined(USE_WX) && USE_WX
	wxGLCanvas *glCanvas;
	wxGLContext *glCtxt;
	wxPanel *panel;
#elif defined(__APPLE__)
	NSWindow *cocoaWin;
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

extern cInterfaceBase *GLInterface;
extern GLWindow GLWin;

#endif
