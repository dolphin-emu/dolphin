// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DolphinWX/GLInterface/EGLX11.h"

EGLDisplay cInterfaceEGLX11::OpenDisplay()
{
	dpy = XOpenDisplay(nullptr);
	XWindow.Initialize(dpy);
	return eglGetDisplay(dpy);
}

EGLNativeWindowType cInterfaceEGLX11::InitializePlatform(EGLNativeWindowType host_window, EGLConfig config)
{
	EGLint vid;
	eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid);

	XVisualInfo visTemplate;
	visTemplate.visualid = vid;

	XVisualInfo *vi;
	int nVisuals;
	vi = XGetVisualInfo(dpy, VisualIDMask, &visTemplate, &nVisuals);

	return (EGLNativeWindowType) XWindow.CreateXWindow((Window) host_window, vi);
}

void cInterfaceEGLX11::ShutdownPlatform()
{
	XWindow.DestroyXWindow();
	XCloseDisplay(dpy);
}

