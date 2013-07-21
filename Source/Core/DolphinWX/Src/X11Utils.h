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

#ifndef __X11UTILS_H_
#define __X11UTILS_H_

#include "Common.h"

#if defined(HAVE_WX) && HAVE_WX
#include <wx/wx.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#endif

#include <X11/Xlib.h>
#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

#include "Core.h"
#include "ConfigManager.h"

// EWMH state actions, see
// http://freedesktop.org/wiki/Specifications/wm-spec?action=show&redirect=Standards%2Fwm-spec
#define _NET_WM_STATE_REMOVE        0    // remove/unset property
#define _NET_WM_STATE_ADD           1    // add/set property
#define _NET_WM_STATE_TOGGLE        2    // toggle property

namespace X11Utils
{

void SendButtonEvent(Display *dpy, int button, int x, int y, bool pressed);
void SendMotionEvent(Display *dpy, int x, int y);
void EWMH_Fullscreen(Display *dpy, int action);
#if defined(HAVE_WX) && HAVE_WX
Window XWindowFromHandle(void *Handle);
Display *XDisplayFromHandle(void *Handle);
#endif

void InhibitScreensaver(Display *dpy, Window win, bool suspend);

#if defined(HAVE_XRANDR) && HAVE_XRANDR
class XRRConfiguration
{
	public:
		XRRConfiguration(Display *_dpy, Window _win);
		~XRRConfiguration();

		void Update();
		void ToggleDisplayMode(bool bFullscreen);
#if defined(HAVE_WX) && HAVE_WX
		void AddResolutions(wxArrayString& arrayStringFor_FullscreenResolution);
#endif

	private:
		Display *dpy;
		Window win;
		int screen;
		XRRScreenResources *screenResources;
		XRROutputInfo *outputInfo;
		XRRCrtcInfo *crtcInfo;
		RRMode fullMode;
		int fb_width, fb_height, fb_width_mm, fb_height_mm;
		int fs_fb_width, fs_fb_height, fs_fb_width_mm, fs_fb_height_mm;
		bool bValid;
		bool bIsFullscreen;
};
#endif

}
#endif

