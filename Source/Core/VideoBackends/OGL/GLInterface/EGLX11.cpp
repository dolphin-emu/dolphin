// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/OGL/GLInterface/EGLX11.h"

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

	XWindowAttributes attribs;
	if (!XGetWindowAttributes(dpy, (Window)host_window, &attribs))
	{
		ERROR_LOG(VIDEO, "Window attribute retrieval failed");
		return 0;
	}

	s_backbuffer_width  = attribs.width;
	s_backbuffer_height = attribs.height;

	return (EGLNativeWindowType) XWindow.CreateXWindow((Window) host_window, vi);
}

void cInterfaceEGLX11::ShutdownPlatform()
{
	XWindow.DestroyXWindow();
	XCloseDisplay(dpy);
}

