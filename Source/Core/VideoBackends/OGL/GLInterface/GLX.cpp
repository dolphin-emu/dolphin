// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "VideoBackends/OGL/GLInterface/GLX.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092

typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSPROC)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef int ( * PFNGLXSWAPINTERVALSGIPROC) (int interval);

static PFNGLXCREATECONTEXTATTRIBSPROC glXCreateContextAttribs = nullptr;
static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = nullptr;

static bool s_glxError;
static int ctxErrorHandler(Display *dpy, XErrorEvent *ev)
{
	s_glxError = true;
	return 0;
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
	glXSwapBuffers(dpy, win);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceGLX::Create(void *window_handle, bool core)
{
	dpy = XOpenDisplay(nullptr);
	int screen = DefaultScreen(dpy);

	// checking glx version
	int glxMajorVersion, glxMinorVersion;
	glXQueryVersion(dpy, &glxMajorVersion, &glxMinorVersion);
	if (glxMajorVersion < 1 || (glxMajorVersion == 1 && glxMinorVersion < 4))
	{
		ERROR_LOG(VIDEO, "glX-Version %d.%d detected, but need at least 1.4",
		          glxMajorVersion, glxMinorVersion);
		return false;
	}

	// loading core context creation function
	glXCreateContextAttribs = (PFNGLXCREATECONTEXTATTRIBSPROC)GetFuncAddress("glXCreateContextAttribsARB");
	if (!glXCreateContextAttribs)
	{
		ERROR_LOG(VIDEO, "glXCreateContextAttribsARB not found, do you support GLX_ARB_create_context?");
		return false;
	}

	// choosing framebuffer
	int visual_attribs[] =
	{
		GLX_X_RENDERABLE    , True,
		GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
		GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_DEPTH_SIZE      , 0,
		GLX_STENCIL_SIZE    , 0,
		GLX_DOUBLEBUFFER    , True,
		None
	};
	int fbcount = 0;
	GLXFBConfig* fbc = glXChooseFBConfig(dpy, screen, visual_attribs, &fbcount);
	if (!fbc || !fbcount)
	{
		ERROR_LOG(VIDEO, "Failed to retrieve a framebuffer config");
		return false;
	}
	fbconfig = *fbc;
	XFree(fbc);

	// Get an appropriate visual
	XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbconfig);

	s_glxError = false;
	XErrorHandler oldHandler = XSetErrorHandler(&ctxErrorHandler);

	// Create a GLX context.
	// We try to get a 3.3 core profile, else we try it with anything we get.
	int context_attribs[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
		GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		GLX_CONTEXT_FLAGS_ARB,         GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
	};
	ctx = nullptr;
	if (core)
	{
		ctx = glXCreateContextAttribs(dpy, fbconfig, 0, True, context_attribs);
		XSync(dpy, False);
	}
	if (!ctx || s_glxError)
	{
		int context_attribs_legacy[] =
		{
			GLX_CONTEXT_MAJOR_VERSION_ARB, 1,
			GLX_CONTEXT_MINOR_VERSION_ARB, 0,
			None
		};
		s_glxError = false;
		ctx = glXCreateContextAttribs(dpy, fbconfig, 0, True, context_attribs_legacy);
		XSync(dpy, False);
		if (!ctx || s_glxError)
		{
			ERROR_LOG(VIDEO, "Unable to create GL context.");
			return false;
		}
	}
	XSetErrorHandler(oldHandler);

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
	XFree(vi);

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

