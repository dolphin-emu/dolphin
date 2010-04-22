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

#include "X11Utils.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#endif

namespace X11Utils
{

void SendClientEvent(const char *message,
	   	int data1, int data2, int data3, int data4)
{
	XEvent event;
	Display *dpy = (Display *)Core::GetWindowHandle();
	Window win = *(Window *)Core::GetXWindow();

	// Init X event structure for client message
	event.xclient.type = ClientMessage;
	event.xclient.format = 32;
	event.xclient.data.l[0] = XInternAtom(dpy, message, False);
	event.xclient.data.l[1] = data1;
	event.xclient.data.l[2] = data2;
	event.xclient.data.l[3] = data3;
	event.xclient.data.l[4] = data4;

	// Send the event
	if (!XSendEvent(dpy, win, False, False, &event))
		ERROR_LOG(VIDEO, "Failed to send message %s to the emulator window.\n", message);
}

void SendKeyEvent(int key)
{
	XEvent event;
	Display *dpy = (Display *)Core::GetWindowHandle();
	Window win = *(Window *)Core::GetXWindow();

	// Init X event structure for key press event
	event.xkey.type = KeyPress;
	// WARNING:  This works for ASCII keys.  If in the future other keys are needed
	// convert with InputCommon::wxCharCodeWXToX from X11InputBase.cpp.
	event.xkey.keycode = XKeysymToKeycode(dpy, key);

	// Send the event
	if (!XSendEvent(dpy, win, False, False, &event))
		ERROR_LOG(VIDEO, "Failed to send key press event to the emulator window.\n");
}

void EWMH_Fullscreen(int action)
{
	_assert_(action == _NET_WM_STATE_REMOVE || action == _NET_WM_STATE_ADD
			|| action == _NET_WM_STATE_TOGGLE);

	Display *dpy = (Display *)Core::GetWindowHandle();
	Window win = *(Window *)Core::GetXWindow();

	// Init X event structure for _NET_WM_STATE_FULLSCREEN client message
	XEvent event;
	event.xclient.type = ClientMessage;
	event.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
	event.xclient.window = win;
	event.xclient.format = 32;
	event.xclient.data.l[0] = action;
	event.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

	// Send the event
	if (!XSendEvent(dpy, DefaultRootWindow(dpy), False,
				SubstructureRedirectMask | SubstructureNotifyMask, &event))
		ERROR_LOG(VIDEO, "Failed to switch fullscreen/windowed mode.\n");
}

#if defined(HAVE_WX) && HAVE_WX
Window XWindowFromHandle(void *Handle)
{
	return GDK_WINDOW_XID(GTK_WIDGET(Handle)->window);
}

Display *XDisplayFromHandle(void *Handle)
{
	return GDK_WINDOW_XDISPLAY(GTK_WIDGET(Handle)->window);
}
#endif

#if defined(HAVE_XRANDR) && HAVE_XRANDR
XRRConfiguration::XRRConfiguration(Display *_dpy, Window _win)
	: dpy(_dpy)
	, win(_win)
	, deskSize(-1), fullSize(-1)
{
	int vidModeMajorVersion, vidModeMinorVersion;
	XRRQueryVersion(dpy, &vidModeMajorVersion, &vidModeMinorVersion);
	NOTICE_LOG(VIDEO, "XRRExtension-Version %d.%d", vidModeMajorVersion, vidModeMinorVersion);
	Update();
}

XRRConfiguration::~XRRConfiguration()
{
	ToggleDisplayMode(False);
	XRRFreeScreenConfigInfo(screenConfig);
}


void XRRConfiguration::Update()
{
	// Get the resolution setings for fullscreen mode
	int fullWidth, fullHeight;
	sscanf(SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution.c_str(),
			"%dx%d", &fullWidth, &fullHeight);

	XRRScreenSize *sizes;
	int numSizes;

	screenConfig = XRRGetScreenInfo(dpy, win);

	/* save desktop resolution */
	deskSize = XRRConfigCurrentConfiguration(screenConfig, &screenRotation);
	/* Set the desktop resolution as the default */
	fullSize = deskSize;

	/* Find the index of the fullscreen resolution from config */
	sizes = XRRConfigSizes(screenConfig, &numSizes);
	if (numSizes > 0 && sizes != NULL) {
		for (int i = 0; i < numSizes; i++) {
			if ((sizes[i].width == fullWidth) && (sizes[i].height == fullHeight)) {
				fullSize = i;
			}
		}
		NOTICE_LOG(VIDEO, "Fullscreen Resolution %dx%d", 
				sizes[fullSize].width, sizes[fullSize].height);
	}
	else {
		ERROR_LOG(VIDEO, "Failed to obtain fullscreen size.\n"
				"Using current desktop resolution for fullscreen.\n");
	}
}

void XRRConfiguration::ToggleDisplayMode(bool bFullscreen)
{
	if (bFullscreen)
		XRRSetScreenConfig(dpy, screenConfig, win,
				fullSize, screenRotation, CurrentTime);
	else
		XRRSetScreenConfig(dpy, screenConfig, win,
				deskSize, screenRotation, CurrentTime);
}

#if defined(HAVE_WX) && HAVE_WX
void XRRConfiguration::AddResolutions(wxArrayString& arrayStringFor_FullscreenResolution)
{
	int screen;
	screen = DefaultScreen(dpy);
	//Get all full screen resos for the config dialog
	XRRScreenSize *sizes = NULL;
	int modeNum = 0;

	sizes = XRRSizes(dpy, screen, &modeNum);
	if (modeNum > 0 && sizes != NULL)
	{
		for (int i = 0; i < modeNum; i++)
			arrayStringFor_FullscreenResolution.Add(wxString::Format(wxT("%dx%d"),
					   	sizes[i].width, sizes[i].height));
	}
}
#endif

#endif

}

