// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Thread.h"

#if USE_EGL
// Currently Wayland/EGL and X11/EGL platforms are supported.
// The platform may be spelected at run time by setting the
// environment variable DOLPHIN_EGL_PLATFORM to "wayland" or "x11".

enum egl_platform {
	EGL_PLATFORM_NONE,
	EGL_PLATFORM_WAYLAND,
	EGL_PLATFORM_X11,
	EGL_PLATFORM_ANDROID
};

#if HAVE_X11
#include "DolphinWX/GLInterface/X11_Util.h"
#endif
#if HAVE_WAYLAND
#include "DolphinWX/GLInterface/Wayland_Util.h"
#endif

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
	bool fullscreen, running;
#endif


#if USE_EGL
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLDisplay egl_dpy;
	enum egl_platform platform;
	EGLNativeWindowType native_window;
#elif HAVE_X11
	GLXContext ctx;
#endif
#if defined(__APPLE__)
	NSView *cocoaWin;
	NSOpenGLContext *cocoaCtx;
#elif HAVE_X11
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
	int x, y;
	unsigned int width, height;
} GLWindow;

extern cInterfaceBase *GLInterface;
extern GLWindow GLWin;
