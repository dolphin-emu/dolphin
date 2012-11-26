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

#include "Globals.h"
#include "VideoConfig.h"
#include "IniFile.h"
#include "Core.h"
#include "Host.h"
#include "VideoBackend.h"
#include "ConfigManager.h"

#include "Render.h"
#include "VertexShaderManager.h"

#include "GLUtil.h"

#if defined(_WIN32)
#include "EmuWindow.h"
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
	[GLWin.cocoaCtx flushBuffer];
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
	// Handled by Host_UpdateTitle()
#elif defined(__APPLE__)
	[GLWin.cocoaWin setTitle: [NSString stringWithUTF8String: text]];
#elif defined(_WIN32)
	TCHAR temp[512];
	swprintf_s(temp, sizeof(temp)/sizeof(TCHAR), _T("%hs"), text);
	EmuWindow::SetWindowText(temp);
#elif defined(HAVE_X11) && HAVE_X11
	// Tell X to ask the window manager to set the window title.
	// (X itself doesn't provide window title functionality.)
	XStoreName(GLWin.dpy, GLWin.win, text);
#endif
}

namespace OGL
{

// Draw messages on top of the screen
unsigned int VideoBackend::PeekMessages()
{
#ifdef _WIN32
	// TODO: peekmessage
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return TRUE;
#else
	return false;
#endif
}

// Show the current FPS
void VideoBackend::UpdateFPSDisplay(const char *text)
{
	char temp[100];
	snprintf(temp, sizeof temp, "%s | OpenGL | %s", scm_rev_str, text);
	OpenGL_SetWindowText(temp);
}

}

#if defined(HAVE_X11) && HAVE_X11
void XEventThread();

void CreateXWindow(void)
{
	Atom wmProtocols[1];

	// Setup window attributes
	GLWin.attr.colormap = XCreateColormap(GLWin.evdpy,
			GLWin.parent, GLWin.vi->visual, AllocNone);
	GLWin.attr.event_mask = KeyPressMask | StructureNotifyMask | FocusChangeMask;
	GLWin.attr.background_pixel = BlackPixel(GLWin.evdpy, GLWin.screen);
	GLWin.attr.border_pixel = 0;

	// Create the window
	GLWin.win = XCreateWindow(GLWin.evdpy, GLWin.parent,
			GLWin.x, GLWin.y, GLWin.width, GLWin.height, 0,
			GLWin.vi->depth, InputOutput, GLWin.vi->visual,
			CWBorderPixel | CWBackPixel | CWColormap | CWEventMask, &GLWin.attr);
	wmProtocols[0] = XInternAtom(GLWin.evdpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.evdpy, GLWin.win, wmProtocols, 1);
	XSetStandardProperties(GLWin.evdpy, GLWin.win, "GPU", "GPU", None, NULL, 0, NULL);
	XMapRaised(GLWin.evdpy, GLWin.win);
	XSync(GLWin.evdpy, True);

	GLWin.xEventThread = std::thread(XEventThread);
}

void DestroyXWindow(void)
{
	XUnmapWindow(GLWin.dpy, GLWin.win);
	GLWin.win = 0;
	if (GLWin.xEventThread.joinable())
		GLWin.xEventThread.join();
	XFreeColormap(GLWin.evdpy, GLWin.attr.colormap);
}

void XEventThread()
{
	// Free look variables
	static bool mouseLookEnabled = false;
	static bool mouseMoveEnabled = false;
	static float lastMouse[2];
	while (GLWin.win)
	{
		XEvent event;
		KeySym key;
		for (int num_events = XPending(GLWin.evdpy); num_events > 0; num_events--)
		{
			XNextEvent(GLWin.evdpy, &event);
			switch(event.type) {
				case KeyPress:
					key = XLookupKeysym((XKeyEvent*)&event, 0);
					switch (key)
					{
						case XK_3:
							OSDChoice = 1;
							// Toggle native resolution
							g_Config.iEFBScale = g_Config.iEFBScale + 1;
							if (g_Config.iEFBScale > 7) g_Config.iEFBScale = 0;
							break;
						case XK_4:
							OSDChoice = 2;
							// Toggle aspect ratio
							g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
							break;
						case XK_5:
							OSDChoice = 3;
							// Toggle EFB copy
							if (!g_Config.bEFBCopyEnable || g_Config.bCopyEFBToTexture)
							{
								g_Config.bEFBCopyEnable ^= true;
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
						default:
							break;
					}
					if (g_Config.bFreeLook)
					{
						static float debugSpeed = 1.0f;
						switch (key)
						{
							case XK_parenleft:
								debugSpeed /= 2.0f;
								break;
							case XK_parenright:
								debugSpeed *= 2.0f;
								break;
							case XK_w:
								VertexShaderManager::TranslateView(0.0f, debugSpeed);
								break;
							case XK_s:
								VertexShaderManager::TranslateView(0.0f, -debugSpeed);
								break;
							case XK_a:
								VertexShaderManager::TranslateView(debugSpeed, 0.0f);
								break;
							case XK_d:
								VertexShaderManager::TranslateView(-debugSpeed, 0.0f);
								break;
							case XK_r:
								VertexShaderManager::ResetView();
								break;
						}
					}
					break;
				case ButtonPress:
					if (g_Config.bFreeLook)
					{
						switch (event.xbutton.button)
						{
							case 2: // Middle button
								lastMouse[0] = event.xbutton.x;
								lastMouse[1] = event.xbutton.y;
								mouseMoveEnabled = true;
								break;
							case 3: // Right button
								lastMouse[0] = event.xbutton.x;
								lastMouse[1] = event.xbutton.y;
								mouseLookEnabled = true;
								break;
						}
					}
					break;
				case ButtonRelease:
					if (g_Config.bFreeLook)
					{
						switch (event.xbutton.button)
						{
							case 2: // Middle button
								mouseMoveEnabled = false;
								break;
							case 3: // Right button
								mouseLookEnabled = false;
								break;
						}
					}
					break;
				case MotionNotify:
					if (g_Config.bFreeLook)
					{
						if (mouseLookEnabled)
						{
							VertexShaderManager::RotateView((event.xmotion.x - lastMouse[0]) / 200.0f,
									(event.xmotion.y - lastMouse[1]) / 200.0f);
							lastMouse[0] = event.xmotion.x;
							lastMouse[1] = event.xmotion.y;
						}

						if (mouseMoveEnabled)
						{
							VertexShaderManager::TranslateView((event.xmotion.x - lastMouse[0]) / 50.0f,
									(event.xmotion.y - lastMouse[1]) / 50.0f);
							lastMouse[0] = event.xmotion.x;
							lastMouse[1] = event.xmotion.y;
						}
					}
					break;
				case ConfigureNotify:
					Window winDummy;
					unsigned int borderDummy, depthDummy;
					XGetGeometry(GLWin.evdpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
							&GLWin.width, &GLWin.height, &borderDummy, &depthDummy);
					s_backbuffer_width = GLWin.width;
					s_backbuffer_height = GLWin.height;
					break;
				case ClientMessage:
					if ((unsigned long) event.xclient.data.l[0] ==
							XInternAtom(GLWin.evdpy, "WM_DELETE_WINDOW", False))
						Host_Message(WM_USER_STOP);
					if ((unsigned long) event.xclient.data.l[0] ==
							XInternAtom(GLWin.evdpy, "RESIZE", False))
						XMoveResizeWindow(GLWin.evdpy, GLWin.win,
								event.xclient.data.l[1], event.xclient.data.l[2],
								event.xclient.data.l[3], event.xclient.data.l[4]);
					break;
				default:
					break;
			}
		}
		Common::SleepCurrentThread(20);
	}
}
#endif

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool OpenGL_Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

#if defined(USE_WX) && USE_WX
	GLWin.panel = (wxPanel *)window_handle;
	GLWin.glCanvas = new wxGLCanvas(GLWin.panel, wxID_ANY, NULL,
		wxPoint(0, 0), wxSize(_twidth, _theight));
	GLWin.glCanvas->Show(true);
	if (GLWin.glCtxt == NULL) // XXX dirty hack
		GLWin.glCtxt = new wxGLContext(GLWin.glCanvas);

#elif defined(__APPLE__)
	NSRect size;
	NSUInteger style = NSMiniaturizableWindowMask;
	NSOpenGLPixelFormatAttribute attr[2] = { NSOpenGLPFADoubleBuffer, 0 };
	NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc]
		initWithAttributes: attr];
	if (fmt == nil) {
		ERROR_LOG(VIDEO, "failed to create pixel format");
		return NULL;
	}

	GLWin.cocoaCtx = [[NSOpenGLContext alloc]
		initWithFormat: fmt shareContext: nil];
	[fmt release];
	if (GLWin.cocoaCtx == nil) {
		ERROR_LOG(VIDEO, "failed to create context");
		return NULL;
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen) {
		size = [[NSScreen mainScreen] frame];
		style |= NSBorderlessWindowMask;
	} else {
		size = NSMakeRect(_tx, _ty, _twidth, _theight);
		style |= NSResizableWindowMask | NSTitledWindowMask;
	}

	GLWin.cocoaWin = [[NSWindow alloc] initWithContentRect: size
		styleMask: style backing: NSBackingStoreBuffered defer: NO];
	if (GLWin.cocoaWin == nil) {
		ERROR_LOG(VIDEO, "failed to create window");
		return NULL;
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen) {
		CGDisplayCapture(CGMainDisplayID());
		[GLWin.cocoaWin setLevel: CGShieldingWindowLevel()];
	}

	[GLWin.cocoaCtx setView: [GLWin.cocoaWin contentView]];
	[GLWin.cocoaWin makeKeyAndOrderFront: nil];

#elif defined(_WIN32)
	window_handle = (void*)EmuWindow::Create((HWND)window_handle, GetModuleHandle(0), _T("Please wait..."));
	if (window_handle == NULL)
	{
		Host_SysMessage("failed to create window");
		return false;
	}

	// Show the window
	EmuWindow::Show();

	PIXELFORMATDESCRIPTOR pfd =         // pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
		1,                              // Version Number
		PFD_DRAW_TO_WINDOW |            // Format Must Support Window
			PFD_SUPPORT_OPENGL |        // Format Must Support OpenGL
			PFD_DOUBLEBUFFER,           // Must Support Double Buffering
		PFD_TYPE_RGBA,                  // Request An RGBA Format
		32,                             // Select Our Color Depth
		0, 0, 0, 0, 0, 0,               // Color Bits Ignored
		0,                              // 8bit Alpha Buffer
		0,                              // Shift Bit Ignored
		0,                              // No Accumulation Buffer
		0, 0, 0, 0,                     // Accumulation Bits Ignored
		24,                             // 24Bit Z-Buffer (Depth Buffer)  
		8,                              // 8bit Stencil Buffer
		0,                              // No Auxiliary Buffer
		PFD_MAIN_PLANE,                 // Main Drawing Layer
		0,                              // Reserved
		0, 0, 0                         // Layer Masks Ignored
	};

	GLuint      PixelFormat;            // Holds The Results After Searching For A Match

	if (!(hDC=GetDC(EmuWindow::GetWnd()))) {
		PanicAlert("(1) Can't create an OpenGL Device context. Fail.");
		return false;
	}
	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd))) {
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

	CreateXWindow();
	window_handle = (void *)GLWin.win;
#endif
	return true;
}

bool OpenGL_MakeCurrent()
{
	// connect the glx-context to the window
#if defined(USE_WX) && USE_WX
	return GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);
#elif defined(__APPLE__)
	[GLWin.cocoaCtx makeCurrentContext];
#elif defined(_WIN32)
	return wglMakeCurrent(hDC, hRC) ? true : false;
#elif defined(HAVE_X11) && HAVE_X11
#if defined(HAVE_WX) && (HAVE_WX)
	Host_GetRenderWindowSize(GLWin.x, GLWin.y,
			(int&)GLWin.width, (int&)GLWin.height);
	XMoveResizeWindow(GLWin.dpy, GLWin.win, GLWin.x, GLWin.y,
			GLWin.width, GLWin.height);
#endif
	return glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
#endif
	return true;
}

// Update window width, size and etc. Called from Render.cpp
void OpenGL_Update()
{
#if defined(USE_WX) && USE_WX
	int width, height;

	GLWin.panel->GetSize(&width, &height);
	if (width == s_backbuffer_width && height == s_backbuffer_height)
		return;

	GLWin.glCanvas->SetFocus();
	GLWin.glCanvas->SetSize(0, 0, width, height);
	GLWin.glCtxt->SetCurrent(*GLWin.glCanvas);
	s_backbuffer_width = width;
	s_backbuffer_height = height;

#elif defined(__APPLE__)
	int width, height;

	width = [[GLWin.cocoaWin contentView] frame].size.width;
	height = [[GLWin.cocoaWin contentView] frame].size.height;
	if (width == s_backbuffer_width && height == s_backbuffer_height)
		return;

	[GLWin.cocoaCtx setView: [GLWin.cocoaWin contentView]];
	[GLWin.cocoaCtx update];
	[GLWin.cocoaCtx makeCurrentContext];
	s_backbuffer_width = width;
	s_backbuffer_height = height;

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
	if (EmuWindow::GetParentWnd() != 0 &&
			(s_backbuffer_width != width || s_backbuffer_height != height) &&
			width >= 4 && height >= 4)
	{
		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
		s_backbuffer_width = width;
		s_backbuffer_height = height;
	}
#endif
}

// Close backend
void OpenGL_Shutdown()
{
#if defined(USE_WX) && USE_WX
	GLWin.glCanvas->Hide();
	// XXX GLWin.glCanvas->Destroy();
	// XXX delete GLWin.glCtxt;
#elif defined(__APPLE__)
	[GLWin.cocoaWin close];
	[GLWin.cocoaCtx clearDrawable];
	[GLWin.cocoaCtx release];
#elif defined(_WIN32)
	if (hRC)
	{
		if (!wglMakeCurrent(NULL, NULL))
			NOTICE_LOG(VIDEO, "Could not release drawing context.");

		if (!wglDeleteContext(hRC))
			ERROR_LOG(VIDEO, "Release Rendering Context Failed.");

		hRC = NULL;
	}

	if (hDC && !ReleaseDC(EmuWindow::GetWnd(), hDC))
	{
		ERROR_LOG(VIDEO, "Release Device Context Failed.");
		hDC = NULL;
	}
	
#elif defined(HAVE_X11) && HAVE_X11
	DestroyXWindow();
	if (GLWin.ctx && !glXMakeCurrent(GLWin.dpy, None, NULL))
		NOTICE_LOG(VIDEO, "Could not release drawing context.");
	if (GLWin.ctx)
	{
		glXDestroyContext(GLWin.dpy, GLWin.ctx);
		XCloseDisplay(GLWin.dpy);
		XCloseDisplay(GLWin.evdpy);
		GLWin.ctx = NULL;
	}
#endif
}

GLuint OpenGL_ReportGLError(const char *function, const char *file, int line)
{
	GLint err = glGetError();
	if (err != GL_NO_ERROR)
	{
		ERROR_LOG(VIDEO, "%s:%d: (%s) OpenGL error 0x%x - %s\n",
				file, line, function, err, gluErrorString(err));
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
		ERROR_LOG(VIDEO, "%s", (char*)pstr);
		ERROR_LOG(VIDEO, "\n");
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
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
				error = "INCOMPLETE_ATTACHMENT_EXT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
				error = "INCOMPLETE_MISSING_ATTACHMENT_EXT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
				error = "INCOMPLETE_DIMENSIONS_EXT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
				error = "INCOMPLETE_FORMATS_EXT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
				error = "INCOMPLETE_DRAW_BUFFER_EXT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
				error = "INCOMPLETE_READ_BUFFER_EXT";
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
				error = "UNSUPPORTED_EXT";
				break;
		}
		ERROR_LOG(VIDEO, "%s:%d: (%s) OpenGL FBO error - %s\n",
				file, line, function, error);
		return false;
	}
	return true;
}

