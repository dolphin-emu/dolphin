// Copyright (C) 2013 Scott Moreau <oreaus@gmail.com>

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
#ifndef _EGLPLATFORM_H_
#define _EGLPLATFORM_H_

#include "Thread.h"
#include "ConfigManager.h"

#if USE_EGL
// We must include wayland-egl.h before egl.h so our
// native types are defined as wayland native types
#if HAVE_WAYLAND && !HAVE_X11
#include <wayland-egl.h>
#endif
#include <EGL/egl.h>
#endif

#if HAVE_X11
#include "X11_Util.h"
#endif
#if HAVE_WAYLAND
#include "Wayland_Util.h"
#endif

#if USE_EGL
// There may be multiple EGL platforms
enum egl_platform {
	EGL_PLATFORM_NONE,
	EGL_PLATFORM_WAYLAND,
	EGL_PLATFORM_X11,
	EGL_PLATFORM_ANDROID
};

class cPlatform
{
private:
#if HAVE_X11
	cXInterface XInterface;
#endif
#if HAVE_WAYLAND
	cWaylandInterface WaylandInterface;
#endif
public:
	enum egl_platform platform;
	bool SelectDisplay(void);
	bool Init(EGLConfig config);
	EGLDisplay EGLGetDisplay(void);
	EGLNativeWindowType CreateWindow(void);
	void DestroyWindow(void);
	void UpdateFPSDisplay(const char *text);
	void ToggleFullscreen(bool fullscreen);
};

#include "GLInterface/EGL.h"

#if HAVE_WAYLAND
#include <wayland-egl.h>
#endif
#elif defined(__APPLE__)
#include "GLInterface/AGL.h"
#elif defined(_WIN32)
#include "GLInterface/WGL.h"
#elif HAVE_X11
#include "GLInterface/GLX.h"
#endif

#if HAVE_WAYLAND
struct geometry {
	int width;
	int height;
};

struct xkb {
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	xkb_mod_mask_t control_mask;
	xkb_mod_mask_t alt_mask;
	xkb_mod_mask_t shift_mask;
};
#endif

typedef struct {
// Currently Wayland/EGL and X11/EGL platforms are supported.
// The platform may be spelected at run time by setting the
// environment variable DOLPHIN_EGL_PLATFORM to "wayland" or "x11".
#if USE_EGL
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLDisplay egl_dpy;
	enum egl_platform platform;
#endif
	EGLNativeWindowType native_window;
#if HAVE_WAYLAND
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_shell *wl_shell;
	struct wl_seat *wl_seat;
	struct {
		struct wl_pointer *wl_pointer;
		uint32_t serial;
	} pointer;
	struct {
		struct wl_keyboard *wl_keyboard;
		struct xkb xkb;
		uint32_t modifiers;
	} keyboard;
	struct wl_shm *wl_shm;
	struct wl_cursor_theme *wl_cursor_theme;
	struct wl_cursor *wl_cursor;
	struct wl_surface *wl_cursor_surface;
	struct geometry geometry, window_size;
	struct wl_egl_window *wl_egl_native;
	struct wl_surface *wl_surface;
	struct wl_shell_surface *wl_shell_surface;
	struct wl_callback *wl_callback;
	bool fullscreen, configured, frame_drawn, swap_complete, running;
#endif
#if HAVE_X11
	int screen;
	// dpy used for egl/glx stuff, evdpy for window events etc.
	// evdpy is to be used by XEventThread only
	Display *dpy;
	Display *evdpy;
#if !USE_EGL
	GLXContext ctx;
#endif
	Window win;
	Window parent;
	XVisualInfo *vi;
	XSetWindowAttributes attr;
	std::thread xEventThread;
	int x, y;
	unsigned int width, height;
#elif defined(ANDROID)
	unsigned int width, height;
#elif defined(__APPLE__)
	NSWindow *cocoaWin;
	NSOpenGLContext *cocoaCtx;
#endif
} GLWindow;

extern cInterfaceBase *GLInterface;
extern GLWindow GLWin;

#endif
