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

#include "Host.h"
#include "RenderBase.h"

#include "../GLInterface.h"
#include "EGL.h"

// Show the current FPS
void cInterfaceEGL::UpdateFPSDisplay(const char *text)
{
	Platform.UpdateFPSDisplay(text);
}
void cInterfaceEGL::Swap()
{
	eglSwapBuffers(GLWin.egl_dpy, GLWin.egl_surf);
}
void cInterfaceEGL::SwapInterval(int Interval)
{
	eglSwapInterval(GLWin.egl_dpy, Interval);
}

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceEGL::Create(void *&window_handle)
{
	const char *s;
	EGLint egl_major, egl_minor;
	EGLConfig config;
	EGLint num_configs;

	// attributes for a visual in RGBA format with at least
	// 8 bits per color and a 24 bit depth buffer
	int attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
#ifdef USE_GLES
#ifdef USE_GLES3
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		
		// OpenGL ES 3 bit is disabled for now, until we have a way to select it from runtime
		// Qualcomm drivers don't even care if it is ES2 or ES3 bit set.
		// Intel drivers /might/ not care, but that code path is untested
		// EGL_RENDERABLE_TYPE, (1 << 6) /* EGL_OPENGL_ES3_BIT */,
#else
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#endif
#else
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
		EGL_NONE };

	static const EGLint ctx_attribs[] = {
#ifdef USE_GLES
		EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
		EGL_NONE
	};

	if(!Platform.SelectDisplay())
		return false;

	GLWin.egl_dpy = Platform.EGLGetDisplay();

	if (!GLWin.egl_dpy) {
		INFO_LOG(VIDEO, "Error: eglGetDisplay() failed\n");
		return false;
	}

	GLWin.platform = Platform.platform;

	if (!eglInitialize(GLWin.egl_dpy, &egl_major, &egl_minor)) {
		INFO_LOG(VIDEO, "Error: eglInitialize() failed\n");
		return false;
	}

#ifdef USE_GLES
	eglBindAPI(EGL_OPENGL_ES_API);
#else
	eglBindAPI(EGL_OPENGL_API);
#endif

	if (!eglChooseConfig( GLWin.egl_dpy, attribs, &config, 1, &num_configs)) {
		INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config\n");
		exit(1);
	}

	if (!Platform.Init(config))
		return false;

	s = eglQueryString(GLWin.egl_dpy, EGL_VERSION);
	INFO_LOG(VIDEO, "EGL_VERSION = %s\n", s);

	s = eglQueryString(GLWin.egl_dpy, EGL_VENDOR);
	INFO_LOG(VIDEO, "EGL_VENDOR = %s\n", s);

	s = eglQueryString(GLWin.egl_dpy, EGL_EXTENSIONS);
	INFO_LOG(VIDEO, "EGL_EXTENSIONS = %s\n", s);

	s = eglQueryString(GLWin.egl_dpy, EGL_CLIENT_APIS);
	INFO_LOG(VIDEO, "EGL_CLIENT_APIS = %s\n", s);

	GLWin.egl_ctx = eglCreateContext(GLWin.egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
	if (!GLWin.egl_ctx) {
		INFO_LOG(VIDEO, "Error: eglCreateContext failed\n");
		exit(1);
	}

	GLWin.native_window = Platform.CreateWindow();

	GLWin.egl_surf = eglCreateWindowSurface(GLWin.egl_dpy, config,
				GLWin.native_window, NULL);
	if (!GLWin.egl_surf) {
		INFO_LOG(VIDEO, "Error: eglCreateWindowSurface failed\n");
		exit(1);
	}

	Platform.ToggleFullscreen(SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);

	window_handle = (void *)GLWin.native_window;
	return true;
}

bool cInterfaceEGL::MakeCurrent()
{
	return eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx);
}
// Close backend
void cInterfaceEGL::Shutdown()
{
	Platform.DestroyWindow();
	if (GLWin.egl_ctx && !eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx))
		NOTICE_LOG(VIDEO, "Could not release drawing context.");
	if (GLWin.egl_ctx)
	{
		eglMakeCurrent(GLWin.egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if(!eglDestroyContext(GLWin.egl_dpy, GLWin.egl_ctx))
			NOTICE_LOG(VIDEO, "Could not destroy drawing context.");
		if(!eglDestroySurface(GLWin.egl_dpy, GLWin.egl_surf))
			NOTICE_LOG(VIDEO, "Could not destroy window surface.");
		if(!eglTerminate(GLWin.egl_dpy))
			NOTICE_LOG(VIDEO, "Could not destroy display connection.");
		GLWin.egl_ctx = NULL;
	}
}
