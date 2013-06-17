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
#include "VideoConfig.h"

#include "../GLInterface.h"
#include "GLX.h"

// Show the current FPS
void cInterfaceGLX::UpdateFPSDisplay(const char *text)
{
	XStoreName(GLWin.evdpy, GLWin.win, text);
}

void cInterfaceGLX::SwapInterval(int Interval)
{
	if (glXSwapIntervalSGI)
		glXSwapIntervalSGI(Interval);
	else
		ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}

void cInterfaceGLX::Swap()
{
	glXSwapBuffers(GLWin.dpy, GLWin.win);
}

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceGLX::Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	int glxMajorVersion, glxMinorVersion;

	// attributes for a single buffered visual in RGBA format with at least
	// 8 bits per color and a 24 bit depth buffer
	int attrListSgl[] = {GLX_RGBA, GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		None};

	// attributes for a double buffered visual in RGBA format with at least
	// 8 bits per color and a 24 bit depth buffer
	int attrListDbl[] = {GLX_RGBA, GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		None };

	int attrListDefault[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None };

	GLWin.dpy = XOpenDisplay(0);
	GLWin.evdpy = XOpenDisplay(0);
	GLWin.parent = (Window)window_handle;
	GLWin.screen = DefaultScreen(GLWin.dpy);
	if (GLWin.parent == 0)
		GLWin.parent = RootWindow(GLWin.dpy, GLWin.screen);

	glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
	NOTICE_LOG(VIDEO, "glX-Version %d.%d", glxMajorVersion, glxMinorVersion);

	// Get an appropriate visual
	GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDbl);
	if (GLWin.vi == NULL)
	{
		GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListSgl);
		if (GLWin.vi != NULL)
		{
			ERROR_LOG(VIDEO, "Only single buffered visual!");
		}
		else
		{
			GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDefault);
			if (GLWin.vi == NULL)
			{
				ERROR_LOG(VIDEO, "Could not choose visual (glXChooseVisual)");
				return false;
			}
		}
	}
	else
		NOTICE_LOG(VIDEO, "Got double buffered visual!");

	// Create a GLX context.
	GLWin.ctx = glXCreateContext(GLWin.dpy, GLWin.vi, 0, GL_TRUE);
	if (!GLWin.ctx)
	{
		PanicAlert("Unable to create GLX context.");
		return false;
	}

	GLWin.x = _tx;
	GLWin.y = _ty;
	GLWin.width = _twidth;
	GLWin.height = _theight;

	XWindow.CreateXWindow();
	window_handle = (void *)GLWin.win;
	return true;
}

bool cInterfaceGLX::MakeCurrent()
{
	// connect the glx-context to the window
	#if defined(HAVE_WX) && (HAVE_WX)
	Host_GetRenderWindowSize(GLWin.x, GLWin.y,
			(int&)GLWin.width, (int&)GLWin.height);
	XMoveResizeWindow(GLWin.evdpy, GLWin.win, GLWin.x, GLWin.y,
			GLWin.width, GLWin.height);
	#endif
	return glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
}

bool cInterfaceGLX::ClearCurrent()
{
	return glXMakeCurrent(GLWin.dpy, None, NULL);
}


// Close backend
void cInterfaceGLX::Shutdown()
{
	XWindow.DestroyXWindow();
	if (GLWin.ctx)
	{
		glXDestroyContext(GLWin.dpy, GLWin.ctx);
		XCloseDisplay(GLWin.dpy);
		XCloseDisplay(GLWin.evdpy);
		GLWin.ctx = NULL;
	}
}

