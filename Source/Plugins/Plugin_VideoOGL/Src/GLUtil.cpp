// Copyright (C) 2003-2008 Dolphin Project.

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
#include "Config.h"
#include "IniFile.h"
#include "svnrev.h"

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

// externals
int gleft, gright, gtop, gbottom;
int nBackbufferWidth, nBackbufferHeight; // screen width
int nXoff, nYoff; // screen offset
float AR; // aspect ratio

#ifndef _WIN32
GLWindow GLWin;
#endif

#if defined(_WIN32)
static HDC         hDC = NULL;       // Private GDI Device Context
static HGLRC       hRC = NULL;       // Permanent Rendering Context
extern HINSTANCE g_hInstance;
#endif

void OpenGL_SwapBuffers()
{
#if USE_SDL
    SDL_GL_SwapBuffers();
#elif defined(HAVE_COCOA) && HAVE_COCOA
    cocoaGLSwap(GLWin.cocoaCtx,GLWin.cocoaWin);
#elif defined(_WIN32)
    SwapBuffers(hDC);
#elif defined(USE_WX) && USE_WX
    GLWin.glCanvas->SwapBuffers();
#elif defined(HAVE_X11) && HAVE_X11
    glXSwapBuffers(GLWin.dpy, GLWin.win);
#endif
}

int OpenGL_GetXoff() {
    return nXoff;
}

int OpenGL_GetYoff() {
    return nYoff;
}

u32 OpenGL_GetWidth() {
    return nBackbufferWidth;
}

u32 OpenGL_GetHeight() {
    return nBackbufferHeight;
}

void OpenGL_SetSize(u32 width, u32 height) {
    nBackbufferWidth = width;
    nBackbufferHeight = height;
}


void OpenGL_SetWindowText(const char *text) 
{
#if USE_SDL
    SDL_WM_SetCaption(text, NULL);
#elif defined(HAVE_COCOA) && HAVE_COCOA
    cocoaGLSetTitle(GLWin.cocoaWin, text);
#elif defined(_WIN32)
    SetWindowText(EmuWindow::GetWnd(), text);
#elif defined(USE_WX) && USE_WX
    GLWin.frame->SetTitle(wxString::FromAscii(text));
#elif defined(HAVE_X11) && HAVE_X11 // GLX
    /**
    * Tell X to ask the window manager to set the window title. (X
    * itself doesn't provide window title functionality.)
    */
    XStoreName(GLWin.dpy, GLWin.win, text); 
#endif
}

unsigned int Callback_PeekMessages()
{
#ifdef _WIN32
    //TODO: peekmessage
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

void UpdateFPSDisplay(const char *text)
{
    char temp[512];
    sprintf(temp, "SVN R%s: GL: %s", SVN_REV_STR, text);
    OpenGL_SetWindowText(temp);

}

// =======================================================================================
// Create window. Called from main.cpp
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, int _iwidth, int _iheight) 
{
    int _twidth,  _theight;
    if(g_Config.bFullscreen)
    {
        if(strlen(g_Config.iFSResolution) > 1)
        {
            sscanf(g_Config.iFSResolution, "%dx%d", &_twidth, &_theight);
        }
        else // No full screen reso set, fall back to default reso
        {
            _twidth = _iwidth;
            _theight = _iheight;
        }
    }
    else // Going Windowed
    {
        if(strlen(g_Config.iWindowedRes) > 1)
        {
            sscanf(g_Config.iWindowedRes, "%dx%d", &_twidth, &_theight);
        }
        else // No Window reso set, fall back to default
        {
            _twidth = _iwidth;
            _theight = _iheight;
        }
    }
#if defined(_WIN32)
    EmuWindow::SetSize(_twidth, _theight);
#endif

	// ---------------------------------------------------------------------------------------
	// Control window size and picture scaling
	// ------------------
	// nBackbufferWidth and nBackbufferHeight = Screen resolution from ini, or 640x480
	// See OpenGL_Update() for documentation of the other variables
	// ------------------
    nBackbufferWidth = _twidth;
    nBackbufferHeight = _theight;

	float FactorW  = 640.0f / (float)nBackbufferWidth;
	float FactorH  = 480.0f / (float)nBackbufferHeight;
	float Max = (FactorW < FactorH) ? FactorH : FactorW;

	if(g_Config.bStretchToFit)
	{
		MValueX = 1.0f / FactorW;
		MValueY = 1.0f / FactorH;
		nXoff = 0;
		nYoff = 0;
	}
	else
	{
		MValueX = 1.0f / Max;
		MValueY = 1.0f / Max;
		nXoff = (int)((nBackbufferWidth - (640 * MValueX)) / 2);
		nYoff = (int)((nBackbufferHeight - (480 * MValueY)) / 2);
	}

    g_VideoInitialize.pPeekMessages = &Callback_PeekMessages;
    g_VideoInitialize.pUpdateFPSDisplay = &UpdateFPSDisplay;

	//char buff[100];
	//sprintf(buff, "%i %i %d %d %d", nBackbufferWidth, nBackbufferHeight, Max, MValueX, MValueY);
	//MessageBox(0, buff, "", 0);

#if USE_SDL
	//init sdl video
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		//TODO : Display an error message
		SDL_Quit();
		return false;
	}

	//setup ogl to use double buffering
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#elif defined(HAVE_COCOA) && HAVE_COCOA
    cocoaGLCreateApp();
    GLWin.width = nBackbufferWidth;
    GLWin.height = nBackbufferHeight;
    GLWin.cocoaWin = cocoaGLCreateWindow(GLWin.width, GLWin.height);
    GLWin.cocoaCtx = cocoaGLInit(g_Config.iMultisampleMode);
#elif defined(USE_WX) && USE_WX
    int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0};

    wxSize size(_iwidth, _iheight);
    if (!g_Config.renderToMainframe || 
        g_VideoInitialize.pWindowHandle == NULL) {
        GLWin.frame = new wxFrame((wxWindow *)g_VideoInitialize.pWindowHandle, 
                                  -1, _("Dolphin"), wxPoint(0,0), size);
    } else {
        GLWin.frame = new wxFrame((wxWindow *)NULL, 
                                  -1, _("Dolphin"), wxPoint(0,0), size);
    }
    GLWin.glCanvas = new wxGLCanvas(GLWin.frame, wxID_ANY, args,
                                    wxPoint(0,0), size, wxSUNKEN_BORDER);
    GLWin.glCtxt = new wxGLContext(GLWin.glCanvas);

    GLWin.frame->Show(TRUE);
    GLWin.glCanvas->Show(TRUE);

    GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);
    //    GLWin.glCtxt->SetCurrent(*GLWin.glCanvas);
#elif defined(_WIN32)
    // create the window
    if (!g_Config.renderToMainframe || g_VideoInitialize.pWindowHandle == NULL)
	{
		g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create(NULL, g_hInstance, "Please wait...");
    }
    else
    {
        g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, "Please wait...");
    }
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
        
    if (g_Config.bFullscreen) {
        //nBackbufferWidth = rcdesktop.right - rcdesktop.left;
        //nBackbufferHeight = rcdesktop.bottom - rcdesktop.top;

        DEVMODE dmScreenSettings;
        memset(&dmScreenSettings,0,sizeof(dmScreenSettings));
        dmScreenSettings.dmSize=sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth    = nBackbufferWidth;
        dmScreenSettings.dmPelsHeight   = nBackbufferHeight;
        dmScreenSettings.dmBitsPerPel   = 32;
        dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

        // Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
        if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
        {
            if (MessageBox(NULL,"The Requested Fullscreen Mode Is Not Supported By\nYour Video Card. Use Windowed Mode Instead?","NeHe GL",MB_YESNO|MB_ICONEXCLAMATION)==IDYES)
                g_Config.bFullscreen = false;
            else
                return false;
        }
    }
    else
	{
        // change to default resolution
        ChangeDisplaySettings(NULL, 0);
    }

    if (g_Config.bFullscreen && !g_Config.renderToMainframe)
	{
        ShowCursor(FALSE);
    }
    else
	{
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW;
    }

    RECT rc;
    rc.left = 0; rc.top = 0;
    rc.right = nBackbufferWidth; rc.bottom = nBackbufferHeight;
    AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle);
    int X = (rcdesktop.right-rcdesktop.left)/2 - (rc.right-rc.left)/2;
    int Y = (rcdesktop.bottom-rcdesktop.top)/2 - (rc.bottom-rc.top)/2;

    SetWindowPos(EmuWindow::GetWnd(), NULL, X, Y, rc.right-rc.left, rc.bottom-rc.top, SWP_NOREPOSITION|SWP_NOZORDER);

    PIXELFORMATDESCRIPTOR pfd=              // pfd Tells Windows How We Want Things To Be
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
        MessageBox(NULL,"(1) Can't Create A GL Device Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
        return false;
    }
    
    if (!(PixelFormat=ChoosePixelFormat(hDC,&pfd))) {
        MessageBox(NULL,"(2) Can't Find A Suitable PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
        return false;
    }

    if (!SetPixelFormat(hDC,PixelFormat,&pfd)) {
        MessageBox(NULL,"(3) Can't Set The PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
        return false;
    }

    if (!(hRC=wglCreateContext(hDC))) {
        MessageBox(NULL,"(4) Can't Create A GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
        return false;
    }

#elif defined(HAVE_X11) && HAVE_X11
    XVisualInfo *vi;
    Colormap cmap;
    int dpyWidth, dpyHeight;
    int glxMajorVersion, glxMinorVersion;
    int vidModeMajorVersion, vidModeMinorVersion;
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
                          GLX_SAMPLE_BUFFERS_ARB, g_Config.iMultisampleMode, GLX_SAMPLES_ARB, 1, None };
    GLWin.dpy = XOpenDisplay(0);
	g_VideoInitialize.pWindowHandle = (HWND)GLWin.dpy;
    GLWin.screen = DefaultScreen(GLWin.dpy);

    GLWin.fs = g_Config.bFullscreen; //Set to setting in Options
    
    /* get an appropriate visual */
    vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDbl);
    if (vi == NULL) {
        vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListSgl);
        GLWin.doubleBuffered = False;
        ERROR_LOG("Only Singlebuffered Visual!\n");
    }
    else {
        GLWin.doubleBuffered = True;
        ERROR_LOG("Got Doublebuffered Visual!\n");
    }

    glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
    ERROR_LOG("glX-Version %d.%d\n", glxMajorVersion, glxMinorVersion);
    /* create a GLX context */
    GLWin.ctx = glXCreateContext(GLWin.dpy, vi, 0, GL_TRUE);
    if(!GLWin.ctx)
    {
        ERROR_LOG("Couldn't Create GLX context.Quit");
        exit(0); // TODO: Don't bring down entire Emu
    }
    /* create a color map */
    cmap = XCreateColormap(GLWin.dpy, RootWindow(GLWin.dpy, vi->screen),
                           vi->visual, AllocNone);
    GLWin.attr.colormap = cmap;
    GLWin.attr.border_pixel = 0;

#if defined(HAVE_XXF86VM) && HAVE_XXF86VM
    // get a connection
    XF86VidModeQueryVersion(GLWin.dpy, &vidModeMajorVersion, &vidModeMinorVersion);

    if (GLWin.fs) {
        
        XF86VidModeModeInfo **modes = NULL;
        int modeNum = 0;
        int bestMode = 0;

        // set best mode to current
        bestMode = 0;
        ERROR_LOG("XF86VidModeExtension-Version %d.%d\n", vidModeMajorVersion, vidModeMinorVersion);
        XF86VidModeGetAllModeLines(GLWin.dpy, GLWin.screen, &modeNum, &modes);
        
        if (modeNum > 0 && modes != NULL) {
            /* save desktop-resolution before switching modes */
            GLWin.deskMode = *modes[0];
            /* look for mode with requested resolution */
            for (int i = 0; i < modeNum; i++) {
                if ((modes[i]->hdisplay == _twidth) && (modes[i]->vdisplay == _theight)) {
                    bestMode = i;
                }
            }    

            XF86VidModeSwitchToMode(GLWin.dpy, GLWin.screen, modes[bestMode]);
            XF86VidModeSetViewPort(GLWin.dpy, GLWin.screen, 0, 0);
            dpyWidth = modes[bestMode]->hdisplay;
            dpyHeight = modes[bestMode]->vdisplay;
            ERROR_LOG("Resolution %dx%d\n", dpyWidth, dpyHeight);
            XFree(modes);
            
            /* create a fullscreen window */
            GLWin.attr.override_redirect = True;
            GLWin.attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | KeyReleaseMask | ButtonReleaseMask | StructureNotifyMask;
            GLWin.win = XCreateWindow(GLWin.dpy, RootWindow(GLWin.dpy, vi->screen),
                                      0, 0, dpyWidth, dpyHeight, 0, vi->depth, InputOutput, vi->visual,
                                      CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect,
                                      &GLWin.attr);
            XWarpPointer(GLWin.dpy, None, GLWin.win, 0, 0, 0, 0, 0, 0);
            XMapRaised(GLWin.dpy, GLWin.win);
            XGrabKeyboard(GLWin.dpy, GLWin.win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            XGrabPointer(GLWin.dpy, GLWin.win, True, ButtonPressMask,
                         GrabModeAsync, GrabModeAsync, GLWin.win, None, CurrentTime);
        }
        else {
            ERROR_LOG("Failed to start fullscreen. If you received the \n"
                      "\"XFree86-VidModeExtension\" extension is missing, add\n"
                      "Load \"extmod\"\n"
                      "to your X configuration file (under the Module Section)\n");
            GLWin.fs = 0;
        }
    }
#endif    
    
    if (!GLWin.fs) {

        //XRootWindow(dpy,screen)
        //int X = (rcdesktop.right-rcdesktop.left)/2 - (rc.right-rc.left)/2;
        //int Y = (rcdesktop.bottom-rcdesktop.top)/2 - (rc.bottom-rc.top)/2;

        // create a window in window mode
        GLWin.attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | KeyReleaseMask | ButtonReleaseMask |
            StructureNotifyMask  | ResizeRedirectMask;
        GLWin.win = XCreateWindow(GLWin.dpy, RootWindow(GLWin.dpy, vi->screen),
                                  0, 0, _twidth, _theight, 0, vi->depth, InputOutput, vi->visual,
                                  CWBorderPixel | CWColormap | CWEventMask, &GLWin.attr);
        // only set window title and handle wm_delete_events if in windowed mode
        wmDelete = XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", True);
        XSetWMProtocols(GLWin.dpy, GLWin.win, &wmDelete, 1);
        XSetStandardProperties(GLWin.dpy, GLWin.win, "GPU",
                                   "GPU", None, NULL, 0, NULL);
        XMapRaised(GLWin.dpy, GLWin.win);
    }
#endif
	return true;
}

bool OpenGL_MakeCurrent()
{
#if USE_SDL
	// Note: The reason for having the call to SDL_SetVideoMode in here instead
	//       of in OpenGL_Create() is that "make current" is part of the video
	//       mode setting and is not available as a separate call in SDL. We
	//       have to do "make current" here because this method runs in the CPU
	//       thread while OpenGL_Create() runs in a diferent thread and "make
	//       current" has to be done in the same thread that will be making
	//       calls to OpenGL.

	// Fetch video info.
	const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo();
        if (!videoInfo) {
		// TODO: Display an error message.
		SDL_Quit();
		return false;
	}
	// Compute video mode flags.
	const int videoFlags = SDL_OPENGL
		| ( videoInfo->hw_available ? SDL_HWSURFACE : SDL_SWSURFACE )
		| ( g_Config.bFullscreen ? SDL_FULLSCREEN : 0);
	// Set vide mode.
	// TODO: Can we use this field or is a separate field needed?
	int _twidth = nBackbufferWidth;
	int _theight = nBackbufferHeight;
	SDL_Surface *screen = SDL_SetVideoMode(_twidth, _theight, 0, videoFlags);
	if (!screen) {
		//TODO : Display an error message
		SDL_Quit();
		return false;
	}
#elif defined(HAVE_COCOA) && HAVE_COCOA
    cocoaGLMakeCurrent(GLWin.cocoaCtx,GLWin.cocoaWin);
#elif defined(_WIN32)
    if (!wglMakeCurrent(hDC,hRC)) {
        MessageBox(NULL,"(5) Can't Activate The GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
        return false;
    }
#elif defined(USE_WX) && USE_WX
    GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);
    return true;
#elif defined(HAVE_X11) && HAVE_X11
    Window winDummy;
    unsigned int borderDummy;
    // connect the glx-context to the window
    glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
    XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
                 &GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth);
    ERROR_LOG("GLWin Depth %d", GLWin.depth);
    if (glXIsDirect(GLWin.dpy, GLWin.ctx)) 
        ERROR_LOG("you have Direct Rendering!");
    else
        ERROR_LOG("no Direct Rendering possible!");

    // better for pad plugin key input (thc)
    XSelectInput(GLWin.dpy, GLWin.win, ExposureMask | KeyPressMask | ButtonPressMask | KeyReleaseMask | ButtonReleaseMask | StructureNotifyMask | EnterWindowMask | LeaveWindowMask |
                 FocusChangeMask );
#endif
	return true;
}


// =======================================================================================
// Update window width, size and etc. Called from Render.cpp
// ----------------
void OpenGL_Update()
{
#if USE_SDL
    SDL_Surface *surface = SDL_GetVideoSurface();
    RECT rcWindow;
    if (!surface) return;
    nBackbufferWidth = surface->w;
    nBackbufferHeight = surface->h;

    rcWindow.right = surface->w;
    rcWindow.bottom = surface->h;

#elif defined(HAVE_COCOA) && HAVE_COCOA
    RECT rcWindow;
    rcWindow.right = GLWin.width;
    rcWindow.bottom = GLWin.height;

#elif defined(USE_WX) && USE_WX
    RECT rcWindow;
    rcWindow.right = GLWin.width;
    rcWindow.bottom = GLWin.height;
    
    // TODO fill in
#elif defined(_WIN32)
	RECT rcWindow;
	if (!EmuWindow::GetParentWnd()) {
		if (!g_Config.bStretchToFit)
			return;
		GetWindowRect(EmuWindow::GetWnd(), &rcWindow);
		rcWindow.top += 25;
	}
	else
	{
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);
	}

	// ---------------------------------------------------------------------------------------
	// Get the new window width and height
	// ------------------
	// See below for documentation
	// ------------------
    int width = rcWindow.right - rcWindow.left;
    int height = rcWindow.bottom - rcWindow.top;

	if (EmuWindow::GetParentWnd() != 0)
		::MoveWindow(EmuWindow::GetWnd(), 0,0,width,height, FALSE);

    nBackbufferWidth = width;
    nBackbufferHeight = height;

#elif defined(HAVE_X11) && HAVE_X11
    // We just check all of our events here
    XEvent event;
    KeySym key;
    static RECT rcWindow;
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
                    else
                        XPutBackEvent(GLWin.dpy, &event);
                }
                break;
            case KeyPress:
                key = XLookupKeysym((XKeyEvent*)&event, 0);
                if(key >= XK_F1 && key <= XK_F9) 
                    FKeyPressed = key - 0xff4e;
                else {
                    if(key == XK_Shift_L || key == XK_Shift_R) 
                        ShiftPressed = true;
                    else if(key == XK_Control_L || key == XK_Control_R)
                        ControlPressed = true;
                    else
                        XPutBackEvent(GLWin.dpy, &event);
                }
                break;
            case ButtonPress:
            case ButtonRelease:
                XPutBackEvent(GLWin.dpy, &event);
                break;
            case ConfigureNotify:
                Window winDummy;
                unsigned int borderDummy;
                XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
                             &GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth);
                nBackbufferWidth = GLWin.width;
                nBackbufferHeight = GLWin.height;
                rcWindow.left = 0;
                rcWindow.top = 0;
                rcWindow.right = GLWin.width;
                rcWindow.bottom = GLWin.height;
                break;
            case ClientMessage: //TODO: We aren't reading this correctly, It could be anything, highest chance is that it's a close event though
                Video_Shutdown(); // Calling from here since returning false does nothing
                return; 
                break;
            default:
                //TODO: Should we put the event back if we don't handle it?
                // I think we handle all the needed ones, the rest shouldn't matter
                // But to be safe, let's but them back anyway
                //XPutBackEvent(GLWin.dpy, &event);
                break;
            }
	}
	return;
#endif

	// ---------------------------------------------------------------------------------------
	// Get the new window width and height
	// ------------------
	// nBackbufferWidth and nBackbufferHeight = now the actual screen size
	// Max = the highest of w and h
	// MValueX and MValueY = used for the picture resolution-change rescaling
	// nXoff and nYoff = controls the picture's position inside the Dolphin window
	// ------------------
	/* MValueX and MValueY will be used in
		TextureMngr and VertexShaderManager: Rescale textures on resolution changes
		BPStructs.cpp: Control glScissor() 
		*/
	// ------------------
    float FactorW  = 640.0f / (float)nBackbufferWidth;
    float FactorH  = 480.0f / (float)nBackbufferHeight;
    float Max = (FactorW < FactorH) ? FactorH : FactorW;
	AR = (float)nBackbufferWidth / (float)nBackbufferHeight;

    if (g_Config.bStretchToFit)
    {
		MValueX = 1;
		MValueY = 1;
        nXoff = 0;
        nYoff = 0;
    }
    else
    {
        MValueX = 1.0f / Max;
        MValueY = 1.0f / Max;
        nXoff = (int)((nBackbufferWidth - (640 * MValueX)) / 2);
        nYoff = (int)((nBackbufferHeight - (480 * MValueY)) / 2);
    }
	
	// tell the debugger
	gleft = rcWindow.left; gright = rcWindow.right;
	gtop = rcWindow.top; gbottom = rcWindow.bottom;
}



// =======================================================================================
// Close plugin
// ----------------
void OpenGL_Shutdown()
{
#if USE_SDL
	SDL_Quit();
#elif defined(HAVE_COCOA) && HAVE_COCOA
        cocoaGLDelete(GLWin.cocoaCtx);
#elif defined(USE_WX) && USE_WX
        delete GLWin.glCanvas;
        delete GLWin.frame;
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
            MessageBox(NULL,"Release Rendering Context Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
        }
        hRC = NULL;                                       // Set RC To NULL
    }

    if (hDC && !ReleaseDC(EmuWindow::GetWnd(),hDC))                  // Are We Able To Release The DC
    {
        MessageBox(NULL,"Release Device Context Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
        hDC = NULL;                                       // Set DC To NULL
    }
#elif defined(HAVE_X11) && HAVE_X11
    if (GLWin.ctx)
    {
        if (!glXMakeCurrent(GLWin.dpy, None, NULL))
        {
            ERROR_LOG("Could not release drawing context.\n");
        }
        XUnmapWindow(GLWin.dpy, GLWin.win);
        glXDestroyContext(GLWin.dpy, GLWin.ctx);
        XCloseDisplay(GLWin.dpy);
        GLWin.ctx = NULL;
    }
#if defined(HAVE_XXF86VM) && HAVE_XXF86VM
    /* switch back to original desktop resolution if we were in fs */
    if (GLWin.dpy != NULL) {
        if (GLWin.fs) {
                XF86VidModeSwitchToMode(GLWin.dpy, GLWin.screen, &GLWin.deskMode);
                XF86VidModeSetViewPort(GLWin.dpy, GLWin.screen, 0, 0);
        }
    }
#endif
#endif
}
