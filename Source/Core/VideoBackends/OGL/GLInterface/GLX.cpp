// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "VideoBackends/OGL/GLInterface/GLX.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

typedef int ( * PFNGLXSWAPINTERVALSGIPROC) (int interval);
static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = nullptr;

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
	glXSwapBuffers(dpy, win);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceGLX::Create(void *window_handle)
{
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

	dpy = XOpenDisplay(nullptr);
	int screen = DefaultScreen(dpy);

	glXQueryVersion(dpy, &glxMajorVersion, &glxMinorVersion);
	NOTICE_LOG(VIDEO, "glX-Version %d.%d", glxMajorVersion, glxMinorVersion);

	// Get an appropriate visual
	vi = glXChooseVisual(dpy, screen, attrListDbl);
	if (vi == nullptr)
	{
		vi = glXChooseVisual(dpy, screen, attrListSgl);
		if (vi != nullptr)
		{
			ERROR_LOG(VIDEO, "Only single buffered visual!");
		}
		else
		{
			vi = glXChooseVisual(dpy, screen, attrListDefault);
			if (vi == nullptr)
			{
				ERROR_LOG(VIDEO, "Could not choose visual (glXChooseVisual)");
				return false;
			}
		}
	}
	else
	{
		NOTICE_LOG(VIDEO, "Got double buffered visual!");
	}

	// Create a GLX context.
	ctx = glXCreateContext(dpy, vi, nullptr, GL_TRUE);
	if (!ctx)
	{
		PanicAlert("Unable to create GLX context.");
		return false;
	}

	XWindow.Initialize(dpy);

	Window parent = (Window)window_handle;

	XWindowAttributes attribs;
	if (!XGetWindowAttributes(dpy, parent, &attribs))
	{
		ERROR_LOG(VIDEO, "Window attribute retrieval failed");
		return false;
	}

	s_backbuffer_width  = attribs.width;
	s_backbuffer_height = attribs.height;

	win = XWindow.CreateXWindow(parent, vi);
	return true;
}

bool cInterfaceGLX::MakeCurrent()
{
	bool success = glXMakeCurrent(dpy, win, ctx);
	if (success)
	{
		// load this function based on the current bound context
		glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)GLInterface->GetFuncAddress("glXSwapIntervalSGI");
	}
	return success;
}

bool cInterfaceGLX::ClearCurrent()
{
	return glXMakeCurrent(dpy, None, nullptr);
}


// Close backend
void cInterfaceGLX::Shutdown()
{
	XWindow.DestroyXWindow();
	if (ctx)
	{
		glXDestroyContext(dpy, ctx);
		XCloseDisplay(dpy);
		ctx = nullptr;
	}
}

