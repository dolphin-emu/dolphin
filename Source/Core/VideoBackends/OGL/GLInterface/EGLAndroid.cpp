// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/Host.h"
#include "VideoBackends/OGL/GLInterface/EGLAndroid.h"

EGLDisplay cInterfaceEGLAndroid::OpenDisplay()
{
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLNativeWindowType cInterfaceEGLAndroid::InitializePlatform(EGLNativeWindowType host_window, EGLConfig config)
{
	EGLint format;
	eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry(host_window, 0, 0, format);

	int none, width, height;
	Host_GetRenderWindowSize(none, none, width, height);
	GLInterface->SetBackBufferDimensions(width, height);

	return host_window;
}

void cInterfaceEGLAndroid::ShutdownPlatform()
{
}

