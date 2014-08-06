// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/Host.h"
#include "DolphinWX/GLInterface/GLInterface.h"
#include "VideoCommon/VideoConfig.h"

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
			0, 0, 1, 1, 0,
			GLWin.vi->depth, InputOutput, GLWin.vi->visual,
			CWBorderPixel | CWBackPixel | CWColormap | CWEventMask, &GLWin.attr);
	wmProtocols[0] = XInternAtom(GLWin.evdpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.evdpy, GLWin.win, wmProtocols, 1);
	XSetStandardProperties(GLWin.evdpy, GLWin.win, "GPU", "GPU", None, nullptr, 0, nullptr);
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
{
	while (GLWin.win)
	{
		XEvent event;
		for (int num_events = XPending(GLWin.evdpy); num_events > 0; num_events--)
		{
			XNextEvent(GLWin.evdpy, &event);
			switch (event.type) {
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
