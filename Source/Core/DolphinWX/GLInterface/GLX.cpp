// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Core/Host.h"

#include "DolphinWX/GLInterface/GLInterface.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

typedef int ( * PFNGLXSWAPINTERVALSGIPROC) (int interval);
PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = nullptr;

// Show the current FPS
void cInterfaceGLX::UpdateFPSDisplay(const std::string& text)
{
	XStoreName(GLWin.evdpy, GLWin.win, text.c_str());
}

void cInterfaceGLX::SwapInterval(int Interval)
{
	if (glXSwapIntervalSGI)
		glXSwapIntervalSGI(Interval);
	else
		ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}
void* cInterfaceGLX::GetFuncAddress(const std::string& name)
{
	return (void*)glXGetProcAddress((const GLubyte*)name.c_str());
}

void cInterfaceGLX::Swap()
{
	glXSwapBuffers(GLWin.dpy, GLWin.win);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceGLX::Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	int glxMajorVersion, glxMinorVersion;

	// attributes for a single buffered visual in RGBA format with at least
	// 8 bits per color
	int attrListSgl[] = {GLX_RGBA, GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		None};

	// attributes for a double buffered visual in RGBA format with at least
	// 8 bits per color
	int attrListDbl[] = {GLX_RGBA, GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		None };

	int attrListDefault[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		None };

	GLWin.dpy = XOpenDisplay(nullptr);
	GLWin.evdpy = XOpenDisplay(nullptr);
	GLWin.parent = (Window)window_handle;
	GLWin.screen = DefaultScreen(GLWin.dpy);
	if (GLWin.parent == 0)
		GLWin.parent = RootWindow(GLWin.dpy, GLWin.screen);

	glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
	NOTICE_LOG(VIDEO, "glX-Version %d.%d", glxMajorVersion, glxMinorVersion);

	// Get an appropriate visual
	GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDbl);
	if (GLWin.vi == nullptr)
	{
		GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListSgl);
		if (GLWin.vi != nullptr)
		{
			ERROR_LOG(VIDEO, "Only single buffered visual!");
		}
		else
		{
			GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDefault);
			if (GLWin.vi == nullptr)
			{
				ERROR_LOG(VIDEO, "Could not choose visual (glXChooseVisual)");
				return false;
			}
		}
	}
	else
		NOTICE_LOG(VIDEO, "Got double buffered visual!");

	// Create a GLX context.
	GLWin.ctx = glXCreateContext(GLWin.dpy, GLWin.vi, nullptr, GL_TRUE);
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

	bool success = glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
	if (success)
	{
		// load this function based on the current bound context
		glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)GLInterface->GetFuncAddress("glXSwapIntervalSGI");
	}
	return success;
}

bool cInterfaceGLX::ClearCurrent()
{
	return glXMakeCurrent(GLWin.dpy, None, nullptr);
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
		GLWin.ctx = nullptr;
	}
}

