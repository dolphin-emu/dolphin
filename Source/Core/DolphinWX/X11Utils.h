// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// HACK: Xlib.h (included from gtk/gdk headers and directly) uses #defines on
// common names such as "Status", "BadRequest" or "Response", causing SFML
// headers to be completely broken.
//
// We work around that issue by including SFML first before X11 headers. This
// is terrible, but such is the life with Xlib.
#include <SFML/Network.hpp> // NOLINT

#if defined(HAVE_WX) && HAVE_WX
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <wx/arrstr.h>
#endif

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <X11/X.h>
#include <X11/Xlib.h>


// EWMH state actions, see
// http://freedesktop.org/wiki/Specifications/wm-spec?action=show&redirect=Standards%2Fwm-spec
#define _NET_WM_STATE_REMOVE        0    // remove/unset property
#define _NET_WM_STATE_ADD           1    // add/set property
#define _NET_WM_STATE_TOGGLE        2    // toggle property

namespace X11Utils
{

void ToggleFullscreen(Display *dpy, Window win);
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
		void AddResolutions(std::vector<std::string>& resos);

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
