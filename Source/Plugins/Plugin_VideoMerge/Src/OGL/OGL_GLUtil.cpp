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

// Common
#include "IniFile.h"
#include "Setup.h"

// VideoCommon
#include "VideoConfig.h"

// OGL
#include "OGL_Render.h"
#include "OGL_GLUtil.h"

#ifdef _WIN32
#include "../EmuWindow.h"
extern HINSTANCE g_hInstance;
#endif

namespace OGL
{

#ifdef _WIN32
static HDC hDC = NULL;       // Private GDI Device Context
static HGLRC hRC = NULL;     // Permanent Rendering Context
#else
GLWindow GLWin;
#endif

// Handles OpenGL and the window

// Window dimensions.
static int s_backbuffer_width;
static int s_backbuffer_height;

void OpenGL_SwapBuffers()
{
#if defined(USE_WX) && USE_WX
	GLWin.glCanvas->SwapBuffers();
#elif defined(__APPLE__)
	cocoaGLSwap(GLWin.cocoaCtx,GLWin.cocoaWin);
#elif defined(_WIN32)
	SwapBuffers(hDC);
#elif defined(HAVE_X11) && HAVE_X11
	glXSwapBuffers(GLWin.dpy, GLWin.win);
#endif
}

u32 OpenGL_GetBackbufferWidth() 
{
	return s_backbuffer_width;
}

u32 OpenGL_GetBackbufferHeight() 
{
	return s_backbuffer_height;
}

void OpenGL_SetWindowText(const char *text)
{
#if defined(USE_WX) && USE_WX
	// GLWin.frame->SetTitle(wxString::FromAscii(text));
#elif defined(__APPLE__)
	cocoaGLSetTitle(GLWin.cocoaWin, text);
#elif defined(_WIN32)
	// TODO convert text to unicode and change SetWindowTextA to SetWindowText
	SetWindowTextA(EmuWindow::GetWnd(), text);
#elif defined(HAVE_X11) && HAVE_X11
	// Tell X to ask the window manager to set the window title.
	// (X itself doesn't provide window title functionality.)
	XStoreName(GLWin.dpy, GLWin.win, text);
#endif
}

#if defined(HAVE_X11) && HAVE_X11
THREAD_RETURN XEventThread(void *pArg);

void CreateXWindow (void)
{
	Atom wmProtocols[1];

	// Setup window attributes
	GLWin.attr.colormap = XCreateColormap(GLWin.dpy,
			GLWin.parent, GLWin.vi->visual, AllocNone);
	GLWin.attr.event_mask = KeyPressMask | StructureNotifyMask | FocusChangeMask;
	GLWin.attr.background_pixel = BlackPixel(GLWin.dpy, GLWin.screen);
	GLWin.attr.border_pixel = 0;

	// Create the window
	GLWin.win = XCreateWindow(GLWin.dpy, GLWin.parent,
			GLWin.x, GLWin.y, GLWin.width, GLWin.height, 0, GLWin.vi->depth, InputOutput, GLWin.vi->visual,
			CWBorderPixel | CWBackPixel | CWColormap | CWEventMask, &GLWin.attr);
	wmProtocols[0] = XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.dpy, GLWin.win, wmProtocols, 1);
	XSetStandardProperties(GLWin.dpy, GLWin.win, "GPU", "GPU", None, NULL, 0, NULL);
	XMapRaised(GLWin.dpy, GLWin.win);
	XSync(GLWin.dpy, True);

	GLWin.xEventThread = new Common::Thread(XEventThread, NULL);
}

void DestroyXWindow(void)
{
	XUnmapWindow(GLWin.dpy, GLWin.win);
	GLWin.win = 0;
	XFreeColormap(GLWin.dpy, GLWin.attr.colormap);
	if (GLWin.xEventThread)
		GLWin.xEventThread->WaitForDeath();
	GLWin.xEventThread = NULL;
}

THREAD_RETURN XEventThread(void *pArg)
{
	while (GLWin.win)
	{
		XEvent event;
		KeySym key;
		for (int num_events = XPending(GLWin.dpy); num_events > 0; num_events--) {
			XNextEvent(GLWin.dpy, &event);
			switch(event.type) {
				case KeyPress:
					key = XLookupKeysym((XKeyEvent*)&event, 0);
					switch (key)
					{
						case XK_3:
							OSDChoice = 1;
							// Toggle native resolution
							if (!(g_Config.bNativeResolution || g_Config.b2xResolution))
								g_Config.bNativeResolution = true;
							else if (g_Config.bNativeResolution && Renderer::AllowCustom())
							{
								g_Config.bNativeResolution = false;
								if (Renderer::Allow2x())
									g_Config.b2xResolution = true;
							}
							else if (Renderer::AllowCustom())
								g_Config.b2xResolution = false;
							break;
						case XK_4:
							OSDChoice = 2;
							// Toggle aspect ratio
							g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
							break;
						case XK_5:
							OSDChoice = 3;
							// Toggle EFB copy
							if (g_Config.bEFBCopyDisable || g_Config.bCopyEFBToTexture)
							{
								g_Config.bEFBCopyDisable = !g_Config.bEFBCopyDisable;
								g_Config.bCopyEFBToTexture = false;
							}
							else
							{
								g_Config.bCopyEFBToTexture = !g_Config.bCopyEFBToTexture;
							}
							break;
						case XK_6:
							OSDChoice = 4;
							g_Config.bDisableFog = !g_Config.bDisableFog;
							break;
						case XK_7:
							OSDChoice = 5;
							g_Config.bDisableLighting = !g_Config.bDisableLighting;
							break;
						default:
							break;
					}
					break;
				case ConfigureNotify:
					Window winDummy;
					unsigned int borderDummy, depthDummy;
					XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
							&GLWin.width, &GLWin.height, &borderDummy, &depthDummy);
					s_backbuffer_width = GLWin.width;
					s_backbuffer_height = GLWin.height;
					break;
				case ClientMessage:
					if ((unsigned long) event.xclient.data.l[0] == XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", False))
						g_VideoInitialize.pCoreMessage(WM_USER_STOP);
					if ((unsigned long) event.xclient.data.l[0] == XInternAtom(GLWin.dpy, "RESIZE", False))
						XMoveResizeWindow(GLWin.dpy, GLWin.win, event.xclient.data.l[1],
							   	event.xclient.data.l[2], event.xclient.data.l[3], event.xclient.data.l[4]);
					break;
				default:
					break;
			}
		}
		Common::SleepCurrentThread(20);
	}
	return 0;
}
#endif

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _iwidth, int _iheight)
{
	int _tx, _ty, _twidth,  _theight;
	g_VideoInitialize.pRequestWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

#if defined(USE_WX) && USE_WX
	int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0};

	wxSize size(_twidth, _theight);

	GLWin.panel = (wxPanel *)g_VideoInitialize.pWindowHandle;

	GLWin.glCanvas = new wxGLCanvas(GLWin.panel, wxID_ANY, args,
			wxPoint(0,0), size, wxSUNKEN_BORDER);
	GLWin.glCtxt = new wxGLContext(GLWin.glCanvas);
	GLWin.glCanvas->Show(TRUE);

	GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);

#elif defined(__APPLE__)
	GLWin.width = s_backbuffer_width;
	GLWin.height = s_backbuffer_height;
	GLWin.cocoaWin = cocoaGLCreateWindow(GLWin.width, GLWin.height);
	GLWin.cocoaCtx = cocoaGLInit(g_Config.iMultisampleMode);

#elif defined(_WIN32)

	PIXELFORMATDESCRIPTOR pfd =              // pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),              // Size Of This Pixel Format Descriptor
		1,                                          // Version Number
		PFD_DRAW_TO_WINDOW |                        // Format Must Support Window
			PFD_SUPPORT_OPENGL |                        // Format Must Support OpenGL
			PFD_DOUBLEBUFFER,                           // Must Support Double Buffering
		PFD_TYPE_RGBA,                              // Request An RGBA Format
		32,                                         // Select Our Color Depth
		0, 0, 0, 0, 0, 0,                           // Color Bits Ignored
		0,                                          // 8bit Alpha Buffer
		0,                                          // Shift Bit Ignored
		0,                                          // No Accumulation Buffer
		0, 0, 0, 0,                                 // Accumulation Bits Ignored
		24,                                         // 24Bit Z-Buffer (Depth Buffer)  
		8,                                          // 8bit Stencil Buffer
		0,                                          // No Auxiliary Buffer
		PFD_MAIN_PLANE,                             // Main Drawing Layer
		0,                                          // Reserved
		0, 0, 0                                     // Layer Masks Ignored
	};

	GLuint      PixelFormat;            // Holds The Results After Searching For A Match

	if (!(hDC=GetDC(EmuWindow::GetWnd()))) {
		PanicAlert("(1) Can't create an OpenGL Device context. Fail.");
		return false;
	}
	if (!(PixelFormat = ChoosePixelFormat(hDC,&pfd))) {
		PanicAlert("(2) Can't find a suitable PixelFormat.");
		return false;
	}
	if (!SetPixelFormat(hDC, PixelFormat, &pfd)) {
		PanicAlert("(3) Can't set the PixelFormat.");
		return false;
	}
	if (!(hRC = wglCreateContext(hDC))) {
		PanicAlert("(4) Can't create an OpenGL rendering context.");
		return false;
	}
	// --------------------------------------

#elif defined(HAVE_X11) && HAVE_X11
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
		GLX_SAMPLE_BUFFERS_ARB, g_Config.iMultisampleMode != MULTISAMPLE_OFF?1:0,
		GLX_SAMPLES_ARB, g_Config.iMultisampleMode != MULTISAMPLE_OFF?1:0, 
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
	GLWin.parent = (Window)g_VideoInitialize.pWindowHandle;
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
			ERROR_LOG(VIDEO, "Only Singlebuffered Visual!");
		}
		else
		{
			GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDefault);
			if (GLWin.vi == NULL)
			{
				ERROR_LOG(VIDEO, "Could not choose visual (glXChooseVisual)");
				exit(0);
			}
		}
	}
	else
		NOTICE_LOG(VIDEO, "Got Doublebuffered Visual!");

	// Create a GLX context.
	GLWin.ctx = glXCreateContext(GLWin.dpy, GLWin.vi, 0, GL_TRUE);
	if (!GLWin.ctx)
	{
		PanicAlert("Couldn't Create GLX context.Quit");
		exit(0); // TODO: Don't bring down entire Emu
	}

	GLWin.x = _tx;
	GLWin.y = _ty;
	GLWin.width = _twidth;
	GLWin.height = _theight;

	CreateXWindow();
	g_VideoInitialize.pWindowHandle = (void *)GLWin.win;
#endif
	return true;
}

bool OpenGL_MakeCurrent()
{
	// connect the glx-context to the window
#if defined(USE_WX) && USE_WX
	GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);
#elif defined(__APPLE__)
	cocoaGLMakeCurrent(GLWin.cocoaCtx,GLWin.cocoaWin);
#elif defined(_WIN32)
	return wglMakeCurrent(hDC,hRC) ? true : false;
#elif defined(HAVE_X11) && HAVE_X11
#if defined(HAVE_WX) && (HAVE_WX)
	g_VideoInitialize.pRequestWindowSize(GLWin.x, GLWin.y, (int&)GLWin.width, (int&)GLWin.height);
	XMoveResizeWindow(GLWin.dpy, GLWin.win, GLWin.x, GLWin.y, GLWin.width, GLWin.height);
#endif
	return glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
#endif
	return true;
}

// Update window width, size and etc. Called from Render.cpp
void OpenGL_Update()
{
#if defined(USE_WX) && USE_WX
	GLWin.glCanvas->GetSize((int *)&GLWin.width, (int *)&GLWin.height);
	s_backbuffer_width = GLWin.width;
	s_backbuffer_height = GLWin.height;
#elif defined(__APPLE__)

	// Is anything needed here?

#elif defined(_WIN32)
	RECT rcWindow;
	if (!EmuWindow::GetParentWnd())
	{
		// We are not rendering to a child window - use client size.
		GetClientRect(EmuWindow::GetWnd(), &rcWindow);
	}
	else
	{
		// We are rendering to a child window - use parent size.
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);
	}

	// Get the new window width and height
	// See below for documentation
	int width = rcWindow.right - rcWindow.left;
	int height = rcWindow.bottom - rcWindow.top;

	// If we are rendering to a child window
	if (EmuWindow::GetParentWnd() != 0 && (s_backbuffer_width != width || s_backbuffer_height != height) && width >= 4 && height >= 4)
	{
		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
		s_backbuffer_width = width;
		s_backbuffer_height = height;
	}
#endif
}


// Close plugin
void OpenGL_Shutdown()
{
#if defined(USE_WX) && USE_WX
	delete GLWin.glCanvas;
#elif defined(__APPLE__)
	cocoaGLDeleteWindow(GLWin.cocoaWin);
	cocoaGLDelete(GLWin.cocoaCtx);

#elif defined(_WIN32)
	if (hRC)                                            // Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL,NULL))                 // Are We Able To Release The DC And RC Contexts?
		{
			// [F|RES]: if this fails i dont see the message box and
			// cant get out of the modal state so i disable it.
			// This function fails only if i render to main window
			// MessageBox(NULL,"Release Of DC And RC Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		}

		if (!wglDeleteContext(hRC))                     // Are We Able To Delete The RC?
		{
			ERROR_LOG(VIDEO, "Release Rendering Context Failed.");
		}
		hRC = NULL;                                       // Set RC To NULL
	}

	if (hDC && !ReleaseDC(EmuWindow::GetWnd(), hDC))      // Are We Able To Release The DC
	{
		ERROR_LOG(VIDEO, "Release Device Context Failed.");
		hDC = NULL;                                       // Set DC To NULL
	}
	EmuWindow::Close();
#elif defined(HAVE_X11) && HAVE_X11
	DestroyXWindow();
	if (GLWin.ctx && !glXMakeCurrent(GLWin.dpy, None, NULL))
		NOTICE_LOG(VIDEO, "Could not release drawing context.");
	if (GLWin.ctx)
	{
		glXDestroyContext(GLWin.dpy, GLWin.ctx);
		XCloseDisplay(GLWin.dpy);
		GLWin.ctx = NULL;
	}
#endif
}

GLuint OpenGL_ReportGLError(const char *function, const char *file, int line)
{
	GLint err = glGetError();
	if (err != GL_NO_ERROR)
	{
		ERROR_LOG(VIDEO, "%s:%d: (%s) OpenGL error 0x%x - %s\n", file, line, function, err, gluErrorString(err));
	}
	return err;
}

void OpenGL_ReportARBProgramError()
{
	const GLubyte* pstr = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
	if (pstr != NULL && pstr[0] != 0)
	{
		GLint loc = 0;
		glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &loc);
		ERROR_LOG(VIDEO, "program error at %d: ", loc);
		ERROR_LOG(VIDEO, (char*)pstr);
		ERROR_LOG(VIDEO, "");
	}
}

bool OpenGL_ReportFBOError(const char *function, const char *file, int line)
{
	unsigned int fbo_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (fbo_status != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		const char *error = "-";
		switch (fbo_status)
		{
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:          error = "INCOMPLETE_ATTACHMENT_EXT"; break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:  error = "INCOMPLETE_MISSING_ATTACHMENT_EXT"; break;
			case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:          error = "INCOMPLETE_DIMENSIONS_EXT"; break;
			case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:             error = "INCOMPLETE_FORMATS_EXT"; break;
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:         error = "INCOMPLETE_DRAW_BUFFER_EXT"; break;
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:         error = "INCOMPLETE_READ_BUFFER_EXT"; break;
			case GL_FRAMEBUFFER_UNSUPPORTED_EXT:                    error = "UNSUPPORTED_EXT"; break;
		}
		ERROR_LOG(VIDEO, "%s:%d: (%s) OpenGL FBO error - %s\n", file, line, function, error);
		return false;
	}
	return true;
}

}
