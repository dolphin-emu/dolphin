// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include "Core/Host.h"
#include "DolphinWX/GLInterface/GLInterface.h"

bool cPlatform::Init(EGLConfig config, void *window_handle)
{
#if HAVE_X11
	if (!XInterface.Initialize(config, window_handle))
		return false;
#elif ANDROID
	EGLint format;
	eglGetConfigAttrib(GLWin.egl_dpy, config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry((EGLNativeWindowType)Host_GetRenderHandle(), 0, 0, format);
	int none, width, height;
	Host_GetRenderWindowSize(none, none, width, height);
	GLInterface->SetBackBufferDimensions(width, height);
#endif
	return true;
}

EGLDisplay cPlatform::EGLGetDisplay(void)
{
#if HAVE_X11
	return (EGLDisplay) XInterface.EGLGetDisplay();
#elif ANDROID
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
	return nullptr;
}

EGLNativeWindowType cPlatform::CreateWindow(void)
{
#if HAVE_X11
	return (EGLNativeWindowType) XInterface.CreateWindow();
#endif
#ifdef ANDROID
	return (EGLNativeWindowType)Host_GetRenderHandle();
#endif
	return 0;
}

void cPlatform::DestroyWindow(void)
{
#if HAVE_X11
	XInterface.DestroyWindow();
#endif
}

void cPlatform::UpdateFPSDisplay(const std::string& text)
{
#if HAVE_X11
	XInterface.UpdateFPSDisplay(text);
#endif
}

void
cPlatform::SwapBuffers()
{
#if HAVE_X11
	XInterface.SwapBuffers();
#elif ANDROID
	eglSwapBuffers(GLWin.egl_dpy, GLWin.egl_surf);
#endif
}
