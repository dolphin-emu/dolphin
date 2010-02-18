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
#include "svnrev.h"
#include "Setup.h"

#include "Render.h"

#if defined(_WIN32)
#include "OS/Win32.h"
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

// Draw messages on top of the screen
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
    sprintf(temp, "SVN R%s: GL: %s", SVN_REV_STR, text);
    OpenGL_SetWindowText(temp);
}

#if defined(HAVE_X11) && HAVE_X11
void CreateXWindow (void)
{
    Atom wmProtocols[3];
    int width, height;

    if (GLWin.fs)
    {
#if defined(HAVE_XRANDR) && HAVE_XRANDR
        if (GLWin.fullSize >= 0)
            XRRSetScreenConfig(GLWin.dpy, GLWin.screenConfig, RootWindow(GLWin.dpy, GLWin.screen),
                    GLWin.fullSize, GLWin.screenRotation, CurrentTime);
#endif
        GLWin.attr.override_redirect = True;
        width = GLWin.fullWidth;
        height = GLWin.fullHeight;
    }
    else
    {
        GLWin.attr.override_redirect = False;
        width = GLWin.winWidth;
        height = GLWin.winHeight;
    }

    // Control window size and picture scaling
    s_backbuffer_width = width;
    s_backbuffer_height = height;

    // create the window
    GLWin.attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
        StructureNotifyMask | ResizeRedirectMask;
    GLWin.win = XCreateWindow(GLWin.dpy, RootWindow(GLWin.dpy, GLWin.vi->screen),
            0, 0, width, height, 0, GLWin.vi->depth, InputOutput, GLWin.vi->visual,
            CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect, &GLWin.attr);
    wmProtocols[0] = XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", True);
    wmProtocols[1] = XInternAtom(GLWin.dpy, "WM_TAKE_FOCUS", True);
    wmProtocols[2] = XInternAtom(GLWin.dpy, "TOGGLE_FULLSCREEN", False);
    XSetWMProtocols(GLWin.dpy, GLWin.win, wmProtocols, 3);
    XSetStandardProperties(GLWin.dpy, GLWin.win, "GPU", "GPU", None, NULL, 0, NULL);
    XMapRaised(GLWin.dpy, GLWin.win);
    if (GLWin.fs)
    {
        XGrabKeyboard(GLWin.dpy, GLWin.win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        XGrabPointer(GLWin.dpy, GLWin.win, True, NULL,
                GrabModeAsync, GrabModeAsync, GLWin.win, None, CurrentTime);
        XSetInputFocus(GLWin.dpy, GLWin.win, RevertToPointerRoot, CurrentTime);
    }
    XSync(GLWin.dpy, True);
}

void DestroyXWindow(void)
{
    if( GLWin.ctx )
    {
        if( !glXMakeCurrent(GLWin.dpy, None, NULL))
        {
            printf("Could not release drawing context.\n");
        }
    }
    /* switch back to original desktop resolution if we were in fullscreen */
    if( GLWin.fs )
    {
        XUngrabKeyboard (GLWin.dpy, CurrentTime);
        XUngrabPointer (GLWin.dpy, CurrentTime);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
        if (GLWin.fullSize >= 0)
            XRRSetScreenConfig(GLWin.dpy, GLWin.screenConfig, RootWindow(GLWin.dpy, GLWin.screen),
                    GLWin.deskSize, GLWin.screenRotation, CurrentTime);
#endif
    }
    XUndefineCursor(GLWin.dpy, GLWin.win);
    XUnmapWindow(GLWin.dpy, GLWin.win);
    XSync(GLWin.dpy, True);
}

void ToggleFullscreenMode (void)
{
    DestroyXWindow();
    GLWin.fs = !GLWin.fs;
    CreateXWindow();
    OpenGL_MakeCurrent();
}
#endif

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _iwidth, int _iheight)
{
#if !defined(HAVE_X11) || !HAVE_X11
	// Check for fullscreen mode
    int _twidth,  _theight;
    if (g_Config.bFullscreen)
    {
        if (strlen(g_Config.cFSResolution) > 1)
        {
            sscanf(g_Config.cFSResolution, "%dx%d", &_twidth, &_theight);
        }
        else // No full screen reso set, fall back to default reso
        {
            _twidth = _iwidth;
            _theight = _iheight;
        }
    }
    else // Going Windowed
    {
        if (strlen(g_Config.cInternalRes) > 1)
        {
            sscanf(g_Config.cInternalRes, "%dx%d", &_twidth, &_theight);
        }
        else // No Window resolution set, fall back to default
        {
            _twidth = _iwidth;
            _theight = _iheight;
        }
    }

	// Control window size and picture scaling
    s_backbuffer_width = _twidth;
    s_backbuffer_height = _theight;
#endif

    g_VideoInitialize.pPeekMessages = &Callback_PeekMessages;
    g_VideoInitialize.pUpdateFPSDisplay = &UpdateFPSDisplay;

#if defined(USE_WX) && USE_WX
    int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0};

    wxSize size(_iwidth, _iheight);
    if (!g_Config.RenderToMainframe ||
        g_VideoInitialize.pWindowHandle == NULL) {
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
    GLWin.cocoaCtx = cocoaGLInit(g_Config.iMultisampleMode);

#elif defined(_WIN32)
	g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, _T("Please wait..."));
	if (g_VideoInitialize.pWindowHandle == NULL)
	{
		g_VideoInitialize.pSysMessage("failed to create window");
		return false;
	}
        
    if (g_Config.bFullscreen)
	{
        DEVMODE dmScreenSettings;
        memset(&dmScreenSettings,0,sizeof(dmScreenSettings));
        dmScreenSettings.dmSize			= sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth    = s_backbuffer_width;
        dmScreenSettings.dmPelsHeight   = s_backbuffer_height;
        dmScreenSettings.dmBitsPerPel   = 32;
        dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

        // Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
        if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
        {
            if (MessageBox(NULL, _T("The Requested Fullscreen Mode Is Not Supported By\nYour Video Card. Use Windowed Mode Instead?"), _T("NeHe GL"),MB_YESNO|MB_ICONEXCLAMATION)==IDYES)
				EmuWindow::ToggleFullscreen(EmuWindow::GetWnd());
            else
                return false;
        }
		else
		{
			// SetWindowPos to the upper-left corner of the screen
			SetWindowPos(EmuWindow::GetWnd(), HWND_TOP, 0, 0, _twidth, _theight, SWP_NOREPOSITION | SWP_NOZORDER);
		}
    }
    else
	{
        // Change to default resolution
        ChangeDisplaySettings(NULL, 0);
    }

	// Show the window
	EmuWindow::Show();

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
  Colormap cmap;
  int glxMajorVersion, glxMinorVersion;
  int vidModeMajorVersion, vidModeMinorVersion;

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
                       GLX_SAMPLE_BUFFERS_ARB, g_Config.iMultisampleMode, GLX_SAMPLES_ARB, 1, None };


  GLWin.dpy = XOpenDisplay(0);
  g_VideoInitialize.pWindowHandle = (HWND)GLWin.dpy;
  GLWin.screen = DefaultScreen(GLWin.dpy);

  // Fullscreen option.
  GLWin.fs = g_Config.bFullscreen; //Set to setting in Options

  /* get an appropriate visual */
  GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDbl);
  if (GLWin.vi == NULL) {
    GLWin.vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListSgl);
    GLWin.doubleBuffered = False;
    ERROR_LOG(VIDEO, "Only Singlebuffered Visual!");
  }
  else {
    GLWin.doubleBuffered = True;
    NOTICE_LOG(VIDEO, "Got Doublebuffered Visual!");
  }

	glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
	NOTICE_LOG(VIDEO, "glX-Version %d.%d", glxMajorVersion, glxMinorVersion);
	// Create a GLX context.
	GLWin.ctx = glXCreateContext(GLWin.dpy, GLWin.vi, 0, GL_TRUE);
	if(!GLWin.ctx)
	{
		PanicAlert("Couldn't Create GLX context.Quit");
		exit(0); // TODO: Don't bring down entire Emu
	}
	// Create a color map.
	cmap = XCreateColormap(GLWin.dpy, RootWindow(GLWin.dpy, GLWin.vi->screen), GLWin.vi->visual, AllocNone);
	GLWin.attr.colormap = cmap;
	GLWin.attr.border_pixel = 0;
	XkbSetDetectableAutoRepeat(GLWin.dpy, True, NULL);

  // Get the resolution setings for both fullscreen and windowed modes
  if (strlen(g_Config.cFSResolution) > 1)
    sscanf(g_Config.cFSResolution, "%dx%d", &GLWin.fullWidth, &GLWin.fullHeight);
  else // No full screen reso set, fall back to desktop resolution
  {
    GLWin.fullWidth = DisplayWidth(GLWin.dpy, GLWin.screen);
    GLWin.fullHeight = DisplayHeight(GLWin.dpy, GLWin.screen);
  }
  if (strlen(g_Config.cInternalRes) > 1)
    sscanf(g_Config.cInternalRes, "%dx%d", &GLWin.winWidth, &GLWin.winHeight);
  else // No Window resolution set, fall back to default
  {
    GLWin.winWidth = _iwidth;
    GLWin.winHeight = _iheight;
  }

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  XRRQueryVersion(GLWin.dpy, &vidModeMajorVersion, &vidModeMinorVersion);
  XRRScreenSize *sizes;
  int numSizes;

  NOTICE_LOG(VIDEO, "XRRExtension-Version %d.%d", vidModeMajorVersion, vidModeMinorVersion);

  GLWin.screenConfig = XRRGetScreenInfo(GLWin.dpy, RootWindow(GLWin.dpy, GLWin.screen));

  /* save desktop resolution */
  GLWin.deskSize = XRRConfigCurrentConfiguration(GLWin.screenConfig, &GLWin.screenRotation);
  /* Set the desktop resolution as the default */
  GLWin.fullSize = -1;

  /* Find the index of the fullscreen resolution from config */
  sizes = XRRConfigSizes(GLWin.screenConfig, &numSizes);
  if (numSizes > 0 && sizes != NULL) {
	  for (int i = 0; i < numSizes; i++) {
		  if ((sizes[i].width == GLWin.fullWidth) && (sizes[i].height == GLWin.fullHeight)) {
			  GLWin.fullSize = i;
		  }
	  }
	  NOTICE_LOG(VIDEO, "Fullscreen Resolution %dx%d", sizes[GLWin.fullSize].width, sizes[GLWin.fullSize].height);
  }
  else {
    ERROR_LOG(VIDEO, "Failed to obtain fullscreen sizes.\n"
            "Using current desktop resolution for fullscreen.\n");
    GLWin.fullWidth = DisplayWidth(GLWin.dpy, GLWin.screen);
    GLWin.fullHeight = DisplayHeight(GLWin.dpy, GLWin.screen);
  }
#else
  GLWin.fullWidth = DisplayWidth(GLWin.dpy, GLWin.screen);
  GLWin.fullHeight = DisplayHeight(GLWin.dpy, GLWin.screen);
#endif

  CreateXWindow();
  g_VideoInitialize.pXWindow = (Window *) &GLWin.win;

  if (g_Config.bHideCursor)
  {
      // make a blank cursor
      Pixmap Blank;
      XColor DummyColor;
      char ZeroData[1] = {0};
      Blank = XCreateBitmapFromData (GLWin.dpy, GLWin.win, ZeroData, 1, 1);
      GLWin.blankCursor = XCreatePixmapCursor(GLWin.dpy, Blank, Blank, &DummyColor, &DummyColor, 0, 0);
      XFreePixmap (GLWin.dpy, Blank);
  }
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

    // Hide the cursor now
    if (g_Config.bHideCursor)
      XDefineCursor (GLWin.dpy, GLWin.win, GLWin.blankCursor);
    
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
    KeySym key;
    static bool ShiftPressed = false;
    static bool ControlPressed = false;
    static int FKeyPressed = -1;
    int num_events;
    for (num_events = XPending(GLWin.dpy);num_events > 0;num_events--) {
        XNextEvent(GLWin.dpy, &event);
        switch(event.type) {
            case KeyRelease:
                key = XLookupKeysym((XKeyEvent*)&event, 0);
                if(key >= XK_F1 && key <= XK_F9) {
                        g_VideoInitialize.pKeyPress(FKeyPressed, ShiftPressed, ControlPressed);
                        FKeyPressed = -1;
                } else {
                    if(key == XK_Shift_L || key == XK_Shift_R)
                        ShiftPressed = false;
                    else if(key == XK_Control_L || key == XK_Control_R)
                        ControlPressed = false;
                }
                break;
            case KeyPress:
                key = XLookupKeysym((XKeyEvent*)&event, 0);
                if(key >= XK_F1 && key <= XK_F9)
                {
                  if(key == XK_F4 && ((event.xkey.state & Mod1Mask) == Mod1Mask))
                    FKeyPressed = 0x1b;
                  else
                    FKeyPressed = key - 0xff4e;
                }
                else if (key == XK_Escape)
                {
                    if (GLWin.fs)
                    {
                        ToggleFullscreenMode();
                        XEvent event;
                        do {
                            XMaskEvent(GLWin.dpy, StructureNotifyMask, &event);
                        } while ( (event.type != MapNotify) || (event.xmap.event != GLWin.win) );
                    }
                    g_VideoInitialize.pKeyPress(0x1c, ShiftPressed, ControlPressed);
                }
                else if (key == XK_Return && ((event.xkey.state & Mod1Mask) == Mod1Mask))
                  ToggleFullscreenMode();
                else {
                    if(key == XK_Shift_L || key == XK_Shift_R)
                        ShiftPressed = true;
                    else if(key == XK_Control_L || key == XK_Control_R)
                        ControlPressed = true;
                }
                break;
            case ButtonPress:
            case ButtonRelease:
                break;
            case FocusIn:
                if (g_Config.bHideCursor)
                  XDefineCursor(GLWin.dpy, GLWin.win, GLWin.blankCursor);
                break;
            case FocusOut:
                if (g_Config.bHideCursor && !GLWin.fs)
                  XUndefineCursor(GLWin.dpy, GLWin.win);
                break;
            case ConfigureNotify:
                Window winDummy;
                unsigned int borderDummy;
                XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
                             &GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth);
                s_backbuffer_width = GLWin.width;
                s_backbuffer_height = GLWin.height;
                // Save windowed mode size for return from fullscreen
                if (!GLWin.fs)
                {
                  GLWin.winWidth = GLWin.width;
                  GLWin.winHeight = GLWin.height;
                }
                break;
            case ClientMessage:
                if ((ulong) event.xclient.data.l[0] == XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", False))
                  g_VideoInitialize.pKeyPress(0x1b, False, False);
                if ((ulong) event.xclient.data.l[0] == XInternAtom(GLWin.dpy, "TOGGLE_FULLSCREEN", False))
                    ToggleFullscreenMode();
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
    DestroyXWindow();
#if defined(HAVE_XRANDR) && HAVE_XRANDR
    if (GLWin.fullSize >= 0)
        XRRFreeScreenConfigInfo(GLWin.screenConfig);
#endif
	if (g_Config.bHideCursor)
      XFreeCursor(GLWin.dpy, GLWin.blankCursor);
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

