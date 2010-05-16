// Copyright (C) 2003-2009 Dolphin Project.

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

#include "main.h"
#include "VideoConfig.h"
#include "IniFile.h"
#include "svnrev.h"
#include "Setup.h"

//#include "Render.h"

#if defined(_WIN32)
#include "Win32.h"
#else
struct RECT
{
    int left, top;
    int right, bottom;
};
#endif

#include "GLUtil.h"

// Handles OpenGL and the window

// Window dimensions.
static int s_backbuffer_width;
static int s_backbuffer_height;

#ifndef _WIN32
GLWindow GLWin;
#endif

#if defined(_WIN32)
static HDC hDC = NULL;       // Private GDI Device Context
static HGLRC hRC = NULL;       // Permanent Rendering Context
extern HINSTANCE g_hInstance;
#endif

void OpenGL_SwapBuffers()
{
#if defined(USE_WX) && USE_WX
    GLWin.glCanvas->SwapBuffers();
#elif defined(HAVE_COCOA) && HAVE_COCOA
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
    GLWin.frame->SetTitle(wxString::FromAscii(text));
#elif defined(HAVE_COCOA) && HAVE_COCOA
    cocoaGLSetTitle(GLWin.cocoaWin, text);
#elif defined(_WIN32)
	// TODO convert text to unicode and change SetWindowTextA to SetWindowText
    SetWindowTextA(EmuWindow::GetWnd(), text);
#elif defined(HAVE_X11) && HAVE_X11 // GLX
    /**
    * Tell X to ask the window manager to set the window title. (X
    * itself doesn't provide window title functionality.)
    */
    XStoreName(GLWin.dpy, GLWin.win, text);
#endif
}

// =======================================================================================
// Draw messages on top of the screen
// ------------------
unsigned int Callback_PeekMessages()
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
    return FALSE;
#endif
}

// Show the current FPS
void UpdateFPSDisplay(const char *text)
{
    char temp[512];
    sprintf(temp, "SVN R%s: SW: %s", SVN_REV_STR, text);
    OpenGL_SetWindowText(temp);
}
// =========================


// =======================================================================================
// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
// ------------------
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _twidth, int _theight)
{
	int xPos, yPos;
	g_VideoInitialize.pRequestWindowSize(xPos, yPos, _twidth, _theight);

	#if defined(_WIN32)
		EmuWindow::SetSize(_twidth, _theight);
	#endif
	// ----------------------------

	// ---------------------------------------------------------------------------------------
	// Control window size and picture scaling
	// ------------------
    s_backbuffer_width = _twidth;
    s_backbuffer_height = _theight;

    g_VideoInitialize.pPeekMessages = &Callback_PeekMessages;
    g_VideoInitialize.pUpdateFPSDisplay = &UpdateFPSDisplay;

#if defined(USE_WX) && USE_WX
    int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0};

    wxSize size(_twidth, _theight);
    if ( g_VideoInitialize.pWindowHandle == NULL) {
        GLWin.frame = new wxFrame((wxWindow *)NULL,
                                  -1, _("Dolphin"), wxPoint(50,50), size);
    } else {
        GLWin.frame = new wxFrame((wxWindow *)g_VideoInitialize.pWindowHandle,
                                  -1, _("Dolphin"), wxPoint(50,50), size);
    }

    GLWin.glCanvas = new wxGLCanvas(GLWin.frame, wxID_ANY, args,
                                    wxPoint(0,0), size, wxSUNKEN_BORDER);
    GLWin.glCtxt = new wxGLContext(GLWin.glCanvas);
    GLWin.frame->Show(TRUE);
    GLWin.glCanvas->Show(TRUE);

    GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);

#elif defined(HAVE_COCOA) && HAVE_COCOA
    GLWin.width = s_backbuffer_width;
    GLWin.height = s_backbuffer_height;
    GLWin.cocoaWin = cocoaGLCreateWindow(GLWin.width, GLWin.height);
    GLWin.cocoaCtx = cocoaGLInit(0);

#elif defined(_WIN32)
	// ---------------------------------------------------------------------------------------
	// Create rendering window in Windows
	// ----------------------
	g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, _T("Please wait..."));

	// Show the window
	EmuWindow::Show();

	if (g_VideoInitialize.pWindowHandle == NULL)
	{
		g_VideoInitialize.pSysMessage("failed to create window");
		return false;
	}

    GLuint      PixelFormat;            // Holds The Results After Searching For A Match
    DWORD       dwExStyle;              // Window Extended Style
    DWORD       dwStyle;                // Window Style

    RECT rcdesktop;
    GetWindowRect(GetDesktopWindow(), &rcdesktop);
        
	dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	dwStyle = WS_OVERLAPPEDWINDOW;

    RECT rc = {0, 0, s_backbuffer_width, s_backbuffer_height};
    AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle);

    int X = (rcdesktop.right-rcdesktop.left)/2 - (rc.right-rc.left)/2;
    int Y = (rcdesktop.bottom-rcdesktop.top)/2 - (rc.bottom-rc.top)/2;

    // EmuWindow::GetWnd() is either the new child window or the new separate window
	SetWindowPos(EmuWindow::GetWnd(), NULL, X, Y, rc.right-rc.left, rc.bottom-rc.top, SWP_NOREPOSITION | SWP_NOZORDER);

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
    XVisualInfo *vi;
    Colormap cmap;
    int glxMajorVersion, glxMinorVersion;
    Atom wmDelete;

    // attributes for a single buffered visual in RGBA format with at least
    // 8 bits per color and a 24 bit depth buffer
    int attrListSgl[] = {GLX_RGBA, GLX_RED_SIZE, 8,
                         GLX_GREEN_SIZE, 8,
                         GLX_BLUE_SIZE, 8,
                         GLX_DEPTH_SIZE, 24,
                         None};

    // attributes for a double buffered visual in RGBA format with at least
    // 8 bits per color and a 24 bit depth buffer
    int attrListDbl[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
                          GLX_RED_SIZE, 8,
                          GLX_GREEN_SIZE, 8,
                          GLX_BLUE_SIZE, 8,
                          GLX_DEPTH_SIZE, 24,
                          GLX_SAMPLE_BUFFERS_ARB, GLX_SAMPLES_ARB, 1, None };

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
	g_VideoInitialize.pWindowHandle = (HWND)GLWin.dpy;
	GLWin.screen = DefaultScreen(GLWin.dpy);

    /* get an appropriate visual */
    vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDbl);
    if (vi == NULL)
	{
        vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListSgl);
		if (vi != NULL)
		{
			ERROR_LOG(VIDEO, "Only Singlebuffered Visual!");
		}
		else
		{
			vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDefault);
			if (vi == NULL)
			{
				ERROR_LOG(VIDEO, "Could not choose visual (glXChooseVisual)");
				exit(0);
			}
		}
    }
    else
        NOTICE_LOG(VIDEO, "Got Doublebuffered Visual!");

	glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
	NOTICE_LOG(VIDEO, "glX-Version %d.%d", glxMajorVersion, glxMinorVersion);
	// Create a GLX context.
	GLWin.ctx = glXCreateContext(GLWin.dpy, vi, 0, GL_TRUE);
	if(!GLWin.ctx)
	{
		PanicAlert("Couldn't Create GLX context.Quit");
		exit(0); // TODO: Don't bring down entire Emu
	}

	if (GLWin.parent == 0)
		GLWin.parent = RootWindow(GLWin.dpy, vi->screen);
	// Create a color map.
	cmap = XCreateColormap(GLWin.dpy, GLWin.parent, vi->visual, AllocNone);
	GLWin.attr.colormap = cmap;
	GLWin.attr.border_pixel = 0;
	XkbSetDetectableAutoRepeat(GLWin.dpy, True, NULL);

	// create a window in window mode
	GLWin.attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
		StructureNotifyMask  | ResizeRedirectMask;
	GLWin.win = XCreateWindow(GLWin.dpy, GLWin.parent,
			xPos, yPos, _twidth, _theight, 0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel | CWColormap | CWEventMask, &GLWin.attr);
	// only set window title and handle wm_delete_events if in windowed mode
	wmDelete = XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.dpy, GLWin.win, &wmDelete, 1);
	XSetStandardProperties(GLWin.dpy, GLWin.win, "GPU",
			"GPU", None, NULL, 0, NULL);
	XMapRaised(GLWin.dpy, GLWin.win);

	g_VideoInitialize.pXWindow = (Window *) &GLWin.win;
#endif
	return true;
}

bool OpenGL_MakeCurrent()
{
#if defined(USE_WX) && USE_WX
    GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);
#elif defined(HAVE_COCOA) && HAVE_COCOA
    cocoaGLMakeCurrent(GLWin.cocoaCtx,GLWin.cocoaWin);
#elif defined(_WIN32)
    if (!wglMakeCurrent(hDC,hRC)) {
        PanicAlert("(5) Can't Activate The GL Rendering Context.");
        return false;
    }
#elif defined(HAVE_X11) && HAVE_X11
    Window winDummy;
    unsigned int borderDummy;
    // connect the glx-context to the window
    glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
    XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
                 &GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth);
    NOTICE_LOG(VIDEO, "GLWin Depth %d", GLWin.depth)
        if (glXIsDirect(GLWin.dpy, GLWin.ctx)) {
            NOTICE_LOG(VIDEO, "detected direct rendering");
        } else {
         ERROR_LOG(VIDEO, "no Direct Rendering possible!");
        }
    
    // better for pad plugin key input (thc)
    XSelectInput(GLWin.dpy, GLWin.win, ExposureMask | KeyPressMask | KeyReleaseMask |
        StructureNotifyMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask );
#endif
	return true;
}
// Update window width, size and etc. Called from Render.cpp
void OpenGL_Update()
{
#if defined(USE_WX) && USE_WX
    RECT rcWindow = {0};
    rcWindow.right = GLWin.width;
    rcWindow.bottom = GLWin.height;
    
    // TODO fill in

#elif defined(HAVE_COCOA) && HAVE_COCOA
	RECT rcWindow = {0};
    rcWindow.right = GLWin.width;
    rcWindow.bottom = GLWin.height;

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

	// ---------------------------------------------------------------------------------------
	// Get the new window width and height
	// ------------------
	// See below for documentation
	// ------------------
    int width = rcWindow.right - rcWindow.left;
    int height = rcWindow.bottom - rcWindow.top;

	// If we are rendering to a child window
	if (EmuWindow::GetParentWnd() != 0)
		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);

    s_backbuffer_width = width;
    s_backbuffer_height = height;

#elif defined(HAVE_X11) && HAVE_X11
    // We just check all of our events here
    XEvent event;
    int num_events;
    for (num_events = XPending(GLWin.dpy);num_events > 0;num_events--) {
        XNextEvent(GLWin.dpy, &event);
        switch(event.type) {
            case ConfigureNotify:
                Window winDummy;
                unsigned int borderDummy;
                XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
                             &GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth);
                s_backbuffer_width = GLWin.width;
                s_backbuffer_height = GLWin.height;
                break;
            case ClientMessage:
                if ((ulong) event.xclient.data.l[0] == XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", False))
                    g_VideoInitialize.pCoreMessage(WM_USER_STOP);
				if ((ulong) event.xclient.data.l[0] == XInternAtom(GLWin.dpy, "RESIZE", False))
					XMoveResizeWindow(GLWin.dpy, GLWin.win, event.xclient.data.l[1],
							event.xclient.data.l[2], event.xclient.data.l[3], event.xclient.data.l[4]);
                return;
                break;
            default:
                break;
            }
	}
	return;
#endif
}


// Close plugin
void OpenGL_Shutdown()
{
#if defined(USE_WX) && USE_WX
	delete GLWin.glCanvas;
	delete GLWin.frame;
#elif defined(HAVE_COCOA) && HAVE_COCOA
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
#elif defined(HAVE_X11) && HAVE_X11
	if (GLWin.ctx)
	{
		if (!glXMakeCurrent(GLWin.dpy, None, NULL))
		{
			ERROR_LOG(VIDEO, "Could not release drawing context.\n");
		}
		XUnmapWindow(GLWin.dpy, GLWin.win);
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
