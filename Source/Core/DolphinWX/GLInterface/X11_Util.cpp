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

#include "Host.h"
#include "VideoConfig.h"
#include "../GLInterface.h"

#if USE_EGL
bool cXInterface::ServerConnect(void)
{
	GLWin.dpy = XOpenDisplay(NULL);

	if (!GLWin.dpy)
		return false;

	return true;
}

bool cXInterface::Initialize(void *config)
{
	int _tx, _ty, _twidth, _theight;
	XVisualInfo  visTemplate;
	int num_visuals;
	EGLint vid;

	if (!GLWin.dpy) {
		printf("Error: couldn't open X display\n");
		return false;
	}

	if (!eglGetConfigAttrib(GLWin.egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
		printf("Error: eglGetConfigAttrib() failed\n");
		exit(1);
	}

	/* The X window visual must match the EGL config */
	visTemplate.visualid = vid;
	GLWin.vi = XGetVisualInfo(GLWin.dpy, VisualIDMask, &visTemplate, &num_visuals);
	if (!GLWin.vi) {
		printf("Error: couldn't get X visual\n");
		exit(1);
	}

	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	GLWin.x = _tx;
	GLWin.y = _ty;
	GLWin.width = _twidth;
	GLWin.height = _theight;

	GLWin.evdpy = XOpenDisplay(NULL);
	GLWin.parent = GLWin.win;
	GLWin.screen = DefaultScreen(GLWin.dpy);

	if (GLWin.parent == 0)
		GLWin.parent = RootWindow(GLWin.dpy, GLWin.screen);

	return true;
}

void *cXInterface::EGLGetDisplay(void)
{
	return eglGetDisplay(GLWin.dpy);
}

void *cXInterface::CreateWindow(void)
{
	Atom wmProtocols[1];

	// Setup window attributes
	GLWin.attr.colormap = XCreateColormap(GLWin.evdpy,
			GLWin.parent, GLWin.vi->visual, AllocNone);
	GLWin.attr.event_mask = KeyPressMask | StructureNotifyMask | FocusChangeMask;
	GLWin.attr.background_pixel = BlackPixel(GLWin.evdpy, GLWin.screen);
	GLWin.attr.border_pixel = 0;

	// Create the window
	GLWin.win = XCreateWindow(GLWin.evdpy, GLWin.parent,
			GLWin.x, GLWin.y, GLWin.width, GLWin.height, 0,
			GLWin.vi->depth, InputOutput, GLWin.vi->visual,
			CWBorderPixel | CWBackPixel | CWColormap | CWEventMask, &GLWin.attr);
	wmProtocols[0] = XInternAtom(GLWin.evdpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.evdpy, GLWin.win, wmProtocols, 1);
	XSetStandardProperties(GLWin.evdpy, GLWin.win, "GPU", "GPU", None, NULL, 0, NULL);
	XMapRaised(GLWin.evdpy, GLWin.win);
	XSync(GLWin.evdpy, True);

	GLWin.xEventThread = std::thread(&cXInterface::XEventThread, this);
	// Control window size and picture scaling
	GLInterface->SetBackBufferDimensions(GLWin.width, GLWin.height);

	return (void *) GLWin.win;
}

void cXInterface::DestroyWindow(void)
{
	XDestroyWindow(GLWin.evdpy, GLWin.win);
	GLWin.win = 0;
	if (GLWin.xEventThread.joinable())
		GLWin.xEventThread.join();
	XFreeColormap(GLWin.evdpy, GLWin.attr.colormap);
}

void cXInterface::UpdateFPSDisplay(const char *text)
{
	XStoreName(GLWin.evdpy, GLWin.win, text);
}

void cXInterface::SwapBuffers()
{
	eglSwapBuffers(GLWin.egl_dpy, GLWin.egl_surf);
}

void cXInterface::XEventThread()
#else
void cX11Window::CreateXWindow(void)
{
	Atom wmProtocols[1];

	// Setup window attributes
	GLWin.attr.colormap = XCreateColormap(GLWin.evdpy,
			GLWin.parent, GLWin.vi->visual, AllocNone);
	GLWin.attr.event_mask = KeyPressMask | StructureNotifyMask | FocusChangeMask;
	GLWin.attr.background_pixel = BlackPixel(GLWin.evdpy, GLWin.screen);
	GLWin.attr.border_pixel = 0;

	// Create the window
	GLWin.win = XCreateWindow(GLWin.evdpy, GLWin.parent,
			GLWin.x, GLWin.y, GLWin.width, GLWin.height, 0,
			GLWin.vi->depth, InputOutput, GLWin.vi->visual,
			CWBorderPixel | CWBackPixel | CWColormap | CWEventMask, &GLWin.attr);
	wmProtocols[0] = XInternAtom(GLWin.evdpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.evdpy, GLWin.win, wmProtocols, 1);
	XSetStandardProperties(GLWin.evdpy, GLWin.win, "GPU", "GPU", None, NULL, 0, NULL);
	XMapRaised(GLWin.evdpy, GLWin.win);
	XSync(GLWin.evdpy, True);

	GLWin.xEventThread = std::thread(&cX11Window::XEventThread, this);
}

void cX11Window::DestroyXWindow(void)
{
	XUnmapWindow(GLWin.evdpy, GLWin.win);
	GLWin.win = 0;
	if (GLWin.xEventThread.joinable())
		GLWin.xEventThread.join();
	XFreeColormap(GLWin.evdpy, GLWin.attr.colormap);
}

void cX11Window::XEventThread()
#endif
{
	while (GLWin.win)
	{
		XEvent event;
		for (int num_events = XPending(GLWin.evdpy); num_events > 0; num_events--)
		{
			XNextEvent(GLWin.evdpy, &event);
			switch(event.type) {
				case ConfigureNotify:
					GLInterface->SetBackBufferDimensions(event.xconfigure.width, event.xconfigure.height);
					break;
				case ClientMessage:
					if ((unsigned long) event.xclient.data.l[0] ==
							XInternAtom(GLWin.evdpy, "WM_DELETE_WINDOW", False))
						Host_Message(WM_USER_STOP);
					break;
				default:
					break;
			}
		}
		Common::SleepCurrentThread(20);
	}
}
