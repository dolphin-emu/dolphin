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
	XStoreName(GLWin.dpy, GLWin.win, text);
}

void cInterfaceEGL::SwapInterval(int Interval)
{
	eglSwapInterval(GLWin.egl_dpy, Interval);
}

void cInterfaceEGL::Swap()
{
	eglSwapBuffers(GLWin.egl_dpy, GLWin.egl_surf);
}

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceEGL::Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	const char *s;
	EGLint egl_major, egl_minor;

	GLWin.dpy = XOpenDisplay(NULL);

	if (!GLWin.dpy) {
		ERROR_LOG(VIDEO, "Error: couldn't open display\n");
		return false;
	}

	GLWin.egl_dpy = eglGetDisplay(GLWin.dpy);
	if (!GLWin.egl_dpy) {
		ERROR_LOG(VIDEO, "Error: eglGetDisplay() failed\n");
		return false;
	}

	if (!eglInitialize(GLWin.egl_dpy, &egl_major, &egl_minor)) {
		ERROR_LOG(VIDEO, "Error: eglInitialize() failed\n");
		return false;
	}

	s = eglQueryString(GLWin.egl_dpy, EGL_VERSION);
	INFO_LOG(VIDEO, "EGL_VERSION = %s\n", s);

	s = eglQueryString(GLWin.egl_dpy, EGL_VENDOR);
	INFO_LOG(VIDEO, "EGL_VENDOR = %s\n", s);

	s = eglQueryString(GLWin.egl_dpy, EGL_EXTENSIONS);
	INFO_LOG(VIDEO, "EGL_EXTENSIONS = %s\n", s);

	s = eglQueryString(GLWin.egl_dpy, EGL_CLIENT_APIS);
	INFO_LOG(VIDEO, "EGL_CLIENT_APIS = %s\n", s);

	// attributes for a visual in RGBA format with at least
	// 8 bits per color and a 24 bit depth buffer
	int attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
#ifdef USE_GLES
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
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
	
	GLWin.evdpy = XOpenDisplay(NULL);
	GLWin.parent = (Window)window_handle;
	GLWin.screen = DefaultScreen(GLWin.dpy);
	if (GLWin.parent == 0)
		GLWin.parent = RootWindow(GLWin.dpy, GLWin.screen);

	XVisualInfo  visTemplate;
	int num_visuals;
	EGLConfig config;
	EGLint num_configs;
	EGLint vid;

	if (!eglChooseConfig( GLWin.egl_dpy, attribs, &config, 1, &num_configs)) {
		ERROR_LOG(VIDEO, "Error: couldn't get an EGL visual config\n");
		return false;
	}

	if (!eglGetConfigAttrib(GLWin.egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
		ERROG_LOG(VIDEO, "Error: eglGetConfigAttrib() failed\n");
		return false;
	}

	/* The X window visual must match the EGL config */
	visTemplate.visualid = vid;
	GLWin.vi = XGetVisualInfo(GLWin.dpy, VisualIDMask, &visTemplate, &num_visuals);
	if (!GLWin.vi) {
		ERROR_LOG(VIDEO, "Error: couldn't get X visual\n");
		return false;
	}
	
	GLWin.x = _tx;
	GLWin.y = _ty;
	GLWin.width = _twidth;
	GLWin.height = _theight;

	XWindow.CreateXWindow();
#ifdef USE_GLES
	eglBindAPI(EGL_OPENGL_ES_API);
#else
	eglBindAPI(EGL_OPENGL_API);
#endif
	GLWin.egl_ctx = eglCreateContext(GLWin.egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
	if (!GLWin.egl_ctx) {
		ERROR_LOG(VIDEO, "Error: eglCreateContext failed\n");
		return false;
	}

	GLWin.egl_surf = eglCreateWindowSurface(GLWin.egl_dpy, config, GLWin.win, NULL);
	if (!GLWin.egl_surf) {
		ERROR_LOG(VIDEO, "Error: eglCreateWindowSurface failed\n");
		return false;
	}

	if (!eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx)) {
		ERROR_LOG(VIDEO, "Error: eglMakeCurrent() failed\n");
		return false;
	}
	

	INFO_LOG(VIDEO, "GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	INFO_LOG(VIDEO, "GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	INFO_LOG(VIDEO, "GL_VERSION: %s\n", glGetString(GL_VERSION));
	INFO_LOG(VIDEO, "GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
	window_handle = (void *)GLWin.win;
	return true;
}

bool cInterfaceEGL::MakeCurrent()
{
	return eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx);
}
// Close backend
void cInterfaceEGL::Shutdown()
{
	XWindow.DestroyXWindow();
	if (GLWin.egl_ctx && !eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx))
		NOTICE_LOG(VIDEO, "Could not release drawing context.");
	if (GLWin.egl_ctx)
	{
		eglDestroyContext(GLWin.egl_dpy, GLWin.egl_ctx);
		eglDestroySurface(GLWin.egl_dpy, GLWin.egl_surf);
		eglTerminate(GLWin.egl_dpy);
		GLWin.egl_ctx = NULL;
	}
}

