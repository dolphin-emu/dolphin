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

// Create offscreen rendering window and its secondary OpenGL context
// This is used for the normal rendering thread with asynchronous timewarp
bool cInterfaceGLX::CreateOffscreen()
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

	dpy_offscreen = XOpenDisplay(nullptr);
	int screen = DefaultScreen(dpy_offscreen);

	glXQueryVersion(dpy_offscreen, &glxMajorVersion, &glxMinorVersion);
	NOTICE_LOG(VIDEO, "glX-Version %d.%d", glxMajorVersion, glxMinorVersion);

	// Get an appropriate visual
	vi = glXChooseVisual(dpy_offscreen, screen, attrListDbl);
	if (vi == nullptr)
	{
		vi = glXChooseVisual(dpy_offscreen, screen, attrListSgl);
		if (vi != nullptr)
		{
			ERROR_LOG(VIDEO, "Only single buffered visual!");
		}
		else
		{
			vi = glXChooseVisual(dpy_offscreen, screen, attrListDefault);
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
	ctx_offscreen = glXCreateContext(dpy_offscreen, vi, nullptr, GL_TRUE);
	if (!ctx_offscreen)
	{
		PanicAlert("Unable to create GLX context.");
		return false;
	}

	XWindow.Initialize(dpy_offscreen);

	s_backbuffer_width  = 640;
	s_backbuffer_height = 480;

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

bool cInterfaceGLX::MakeCurrentOffscreen()
{
	bool success;
	if (dpy_offscreen && ctx_offscreen)
		success = glXMakeCurrent(dpy_offscreen, win, ctx_offscreen) ? true : false;
	else
		return MakeCurrent();
	if (success)
	{
		// Grab the swap interval function pointer
		glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)GLInterface->GetFuncAddress("glXSwapIntervalSGI");
	}
	return success;
}

bool cInterfaceGLX::ClearCurrent()
{
	return glXMakeCurrent(dpy, None, nullptr);
}

bool cInterfaceGLX::ClearCurrentOffscreen() {
	if(dpy_offscreen)
		return glXMakeCurrent(dpy_offscreen, None, nullptr);
	else
		return ClearCurrent();
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

void cInterfaceGLX::ShutdownOffscreen()
{
	XWindow.DestroyXWindow();
	if (ctx_offscreen)
	{
		glXDestroyContext(dpy_offscreen, ctx_offscreen);
		XCloseDisplay(dpy_offscreen);
		ctx_offscreen = nullptr;
	}
}

