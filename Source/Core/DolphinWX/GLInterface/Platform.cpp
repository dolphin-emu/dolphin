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

#include "Host.h"
#include "Platform.h"

bool cPlatform::SelectDisplay(void)
{
	enum egl_platform selected_platform = EGL_PLATFORM_NONE;
	enum egl_platform desired_platform = EGL_PLATFORM_NONE;
	char *platform_env = getenv("DOLPHIN_EGL_PLATFORM");

	if (platform_env)
		platform_env = strdup(platform_env);

	if (!platform_env)
		printf("Running automatic platform detection\n");

	// Try to select the platform set in the environment variable first
#if HAVE_WAYLAND
	bool wayland_possible = WaylandInterface.ServerConnect();

	if (platform_env && !strcmp(platform_env, "wayland"))
	{
		desired_platform = EGL_PLATFORM_WAYLAND;
		if (wayland_possible)
			selected_platform = EGL_PLATFORM_WAYLAND;
		else
			printf("Wayland display server connection failed\n");
	}
#endif

#if HAVE_X11
	bool x11_possible = XInterface.ServerConnect();

	if (platform_env && !strcmp(platform_env, "x11"))
	{
		desired_platform = EGL_PLATFORM_X11;
		if ((selected_platform != EGL_PLATFORM_WAYLAND) && x11_possible)
			selected_platform = EGL_PLATFORM_X11;
		else
			printf("X11 display server connection failed\n");
	}
#endif
	// Fall back to automatic detection
	if (selected_platform == EGL_PLATFORM_NONE)
	{
		if (platform_env && (desired_platform == EGL_PLATFORM_NONE)) {
			printf("DOLPHIN_EGL_PLATFORM set to unrecognized platform \"%s\".\n"
#if HAVE_WAYLAND && !HAVE_X11
				"Note: Valid value is \"wayland\" (built without x11 support)\n",
#endif
#if HAVE_X11 && !HAVE_WAYLAND
				"Note: Valid values is \"x11\" (built without wayland support)\n",
#endif
#if HAVE_WAYLAND && HAVE_X11
				"Note: Valid values are \"wayland\" and \"x11\"\n",
#endif
#if !HAVE_WAYLAND && !HAVE_X11
				"Note: No Valid platform. Must be Android\n",
#endif
			platform_env);
			free(platform_env);
			platform_env = NULL;
		}
#if HAVE_WAYLAND
		if (wayland_possible)
		{
			selected_platform = EGL_PLATFORM_WAYLAND;
			platform_env = strdup("wayland");
		}
#endif
#if HAVE_X11
		if ((selected_platform != EGL_PLATFORM_WAYLAND) && x11_possible)
		{
			selected_platform = EGL_PLATFORM_X11;
			platform_env = strdup("x11");
		}
#endif
#ifdef ANDROID
		selected_platform = EGL_PLATFORM_ANDROID;
#endif
		if (selected_platform == EGL_PLATFORM_NONE)
		{
			printf("FATAL: Failed to find suitable platform for display connection\n");
			goto out;
		}
	}

	printf("Using EGL Native Display Platform: %s\n", platform_env);
out:
	cPlatform::platform = selected_platform;
	free(platform_env);
#if HAVE_WAYLAND
	if (selected_platform != EGL_PLATFORM_WAYLAND) {
		if (GLWin.wl_display)
			wl_display_disconnect(GLWin.wl_display);
	}

#endif
#if HAVE_X11
	if (selected_platform != EGL_PLATFORM_X11) {
		if (GLWin.dpy)
		{
			XCloseDisplay(GLWin.dpy);
		}
	}
#endif
	if (selected_platform == EGL_PLATFORM_NONE)
		return false;

	return true;
}

bool cPlatform::Init(EGLConfig config)
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		if (!WaylandInterface.Initialize(config))
			return false;
#endif
#if HAVE_X11
	if (cPlatform::platform == EGL_PLATFORM_X11)
		if (!XInterface.Initialize(config))
			return false;
#endif
#ifdef ANDROID
	EGLint format;
	eglGetConfigAttrib(GLWin.egl_dpy, config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry((EGLNativeWindowType)Host_GetRenderHandle(), 0, 0, format);
	int none, width, height;
	Host_GetRenderWindowSize(none, none, width, height);
	GLWin.width = width;
	GLWin.height = height;
	GLInterface->SetBackBufferDimensions(width, height);
#endif
	return true;
}

EGLDisplay cPlatform::EGLGetDisplay(void)
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		return (EGLDisplay) WaylandInterface.EGLGetDisplay();
#endif
#if HAVE_X11
	if (cPlatform::platform == EGL_PLATFORM_X11)
		return (EGLDisplay) XInterface.EGLGetDisplay();
#endif
#ifdef ANDROID
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
	return NULL;
}

EGLNativeWindowType cPlatform::CreateWindow(void)
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		return (EGLNativeWindowType) WaylandInterface.CreateWindow();
#endif
#if HAVE_X11
	if (cPlatform::platform == EGL_PLATFORM_X11)
		return (EGLNativeWindowType) XInterface.CreateWindow();
#endif
#ifdef ANDROID
	return (EGLNativeWindowType)Host_GetRenderHandle();
#endif
	return 0;
}

void cPlatform::DestroyWindow(void)
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		WaylandInterface.DestroyWindow();
#endif
#if HAVE_X11
	if (cPlatform::platform == EGL_PLATFORM_X11)
		XInterface.DestroyWindow();
#endif
}

void cPlatform::UpdateFPSDisplay(const char *text)
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		WaylandInterface.UpdateFPSDisplay(text);
#endif
#if HAVE_X11
	if (cPlatform::platform == EGL_PLATFORM_X11)
		XInterface.UpdateFPSDisplay(text);
#endif
}

void
cPlatform::ToggleFullscreen(bool fullscreen)
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		WaylandInterface.ToggleFullscreen(fullscreen);
#endif
#if HAVE_X11
	// Only wayland uses this function
#endif
}

void
cPlatform::SwapBuffers()
{
#if HAVE_WAYLAND
	if (cPlatform::platform == EGL_PLATFORM_WAYLAND)
		WaylandInterface.SwapBuffers();
#endif
#if HAVE_X11
	if (cPlatform::platform == EGL_PLATFORM_X11)
		XInterface.SwapBuffers();
#endif
}
