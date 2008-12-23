#include "X11Window.h"

X11Window::X11Window(int _iwidth, int _iheight) {
    int _twidth,  _theight;
    if(g_Config.bFullscreen) {
	if(strlen(g_Config.iFSResolution) > 1) {
	    sscanf(g_Config.iFSResolution, "%dx%d", &_twidth, &_theight);
        }
        else { // No full screen reso set, fall back to default {
            _twidth = _iwidth;
            _theight = _iheight;
        }
    } else  {// Going Windowed
        if(strlen(g_Config.iWindowedRes) > 1) {
	    sscanf(g_Config.iWindowedRes, "%dx%d", &_twidth, &_theight);
        }
        else { // No Window reso set, fall back to default
            _twidth = _iwidth;
            _theight = _iheight;
        }
    }
    
    SetSize(_twidth, _theight);

    float FactorW  = 640.0f / (float)_twidth;
    float FactorH  = 480.0f / (float)_theight;
    float Max = (FactorW < FactorH) ? FactorH : FactorW;
    
    if(g_Config.bStretchToFit) {
	SetMax(1.0f / FactorW, 1.0f / FactorH);
	SetOffset(0,0);
    } else {
	SetMax(1.0f / Max, 1.0f / Max);
	SetOffset((int)((_twidth - (640 * GetXmax())) / 2),
		  (int)((_theight - (480 * GetYmax())) / 2));
    }

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
                          GLX_SAMPLE_BUFFERS_ARB, 
			  g_Config.iMultisampleMode, 
			  GLX_SAMPLES_ARB, 1, None };
    dpy = XOpenDisplay(0);
    g_VideoInitialize.pWindowHandle = (HWND)dpy;
    screen = DefaultScreen(dpy);
    
    fs = g_Config.bFullscreen; //Set to setting in Options
    
    /* get an appropriate visual */
    vi = glXChooseVisual(dpy, screen, attrListDbl);
    if (vi == NULL) {
        vi = glXChooseVisual(dpy, screen, attrListSgl);
        doubleBuffered = False;
        ERROR_LOG("Only Singlebuffered Visual!\n");
    } else {
        doubleBuffered = True;
        ERROR_LOG("Got Doublebuffered Visual!\n");
    }
    
    glXQueryVersion(dpy, &glxMajorVersion, &glxMinorVersion);
    ERROR_LOG("glX-Version %d.%d\n", glxMajorVersion, glxMinorVersion);
    /* create a GLX context */
    ctx = glXCreateContext(dpy, vi, 0, GL_TRUE);
    if(!ctx) {
        ERROR_LOG("Couldn't Create GLX context.Quit");
        exit(0); // TODO: Don't bring down entire Emu
    }

    /* create a color map */
    cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                           vi->visual, AllocNone);
    attr.colormap = cmap;
    attr.border_pixel = 0;

    XkbSetDetectableAutoRepeat(dpy, True, NULL);

    // get a connection
    XF86VidModeQueryVersion(dpy, &vidModeMajorVersion, &vidModeMinorVersion);

    if (fs) {
        
        XF86VidModeModeInfo **modes = NULL;
        int modeNum = 0;
        int bestMode = 0;
	
        // set best mode to current
        bestMode = 0;
        ERROR_LOG("XF86VidModeExtension-Version %d.%d\n", vidModeMajorVersion, vidModeMinorVersion);
        XF86VidModeGetAllModeLines(dpy, screen, &modeNum, &modes);
        
        if (modeNum > 0 && modes != NULL) {
            /* save desktop-resolution before switching modes */
            deskMode = *modes[0];
            /* look for mode with requested resolution */
            for (int i = 0; i < modeNum; i++) {
                if ((modes[i]->hdisplay == _twidth) && 
		    (modes[i]->vdisplay == _theight)) {
                    bestMode = i;
                }
            }    
	    
            XF86VidModeSwitchToMode(dpy, screen, modes[bestMode]);
            XF86VidModeSetViewPort(dpy, screen, 0, 0);
            dpyWidth = modes[bestMode]->hdisplay;
            dpyHeight = modes[bestMode]->vdisplay;
            ERROR_LOG("Resolution %dx%d\n", dpyWidth, dpyHeight);
            XFree(modes);
            
            /* create a fullscreen window */
            attr.override_redirect = True;
            attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | 
		KeyReleaseMask | ButtonReleaseMask | StructureNotifyMask;
            win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 
				dpyWidth, dpyHeight, 0, vi->depth, InputOutput,
				vi->visual, CWBorderPixel | CWColormap | 
				CWEventMask | CWOverrideRedirect, &attr);
            XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
            XMapRaised(dpy, win);
            XGrabKeyboard(dpy, win, True, GrabModeAsync, 
			  GrabModeAsync, CurrentTime);
            XGrabPointer(dpy, win, True, ButtonPressMask,
                         GrabModeAsync, GrabModeAsync, win, None, CurrentTime);
        }
        else {
            ERROR_LOG("Failed to start fullscreen. If you received the \n"
                      "\"XFree86-VidModeExtension\" extension is missing, add\n"
                      "Load \"extmod\"\n"
                      "to your X configuration file (under the Module Section)\n");
            fs = 0;
        }
    }
    
    if (!fs) {
	
        // create a window in window mode
        attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | 
	    KeyReleaseMask | ButtonReleaseMask |
            StructureNotifyMask  | ResizeRedirectMask;
        win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, _twidth, 
			    _theight, 0, vi->depth, InputOutput, vi->visual,
			    CWBorderPixel | CWColormap | CWEventMask, &attr);
        // only set window title and handle wm_delete_events if in windowed mode
        wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
        XSetWMProtocols(dpy, win, &wmDelete, 1);
        XSetStandardProperties(dpy, win, "GPU",
			       "GPU", None, NULL, 0, NULL);
        XMapRaised(dpy, win);
    }
}

X11Window::~X11Window() {
    if (ctx) {
        if (!glXMakeCurrent(dpy, None, NULL)) {
            ERROR_LOG("Could not release drawing context.\n");
        }
        XUnmapWindow(dpy, win);
        glXDestroyContext(dpy, ctx);
        XCloseDisplay(dpy);
        ctx = NULL;
    }
    /* switch back to original desktop resolution if we were in fs */
    if (dpy != NULL) {
        if (fs) {
	    XF86VidModeSwitchToMode(dpy, screen, &deskMode);
	    XF86VidModeSetViewPort(dpy, screen, 0, 0);
        }
    }
}

void X11Window::SwapBuffers() {
    glXSwapBuffers(dpy, win);
}

void X11Window::SetWindowText(const char *text) {
    /**
     * Tell X to ask the window manager to set the window title. (X
     * itself doesn't provide window title functionality.)
     */
    XStoreName(dpy, win, text);   
}

bool X11Window::PeekMessages() {
    // TODO: implement
    return false;
}
void X11Window::Update() {
    // We just check all of our events here
    XEvent event;
    KeySym key;
    static bool ShiftPressed = false;
    static bool ControlPressed = false;
    static int FKeyPressed = -1;
    int x,y;
    u32 w,h,depth;

    int num_events;
    for (num_events = XPending(dpy);num_events > 0;num_events--) {
        XNextEvent(dpy, &event);
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
		    XPutBackEvent(dpy, &event);
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
		    XPutBackEvent(dpy, &event);
	    }
	    break;
	case ButtonPress:
	case ButtonRelease:
	    XPutBackEvent(dpy, &event);
	    break;
	case ConfigureNotify:
	    Window winDummy;
	    unsigned int borderDummy;
	    XGetGeometry(dpy, win, &winDummy, &x, &y,
			 &w, &h, &borderDummy, &depth);
	    SetSize(w, h);
	    break;
	case ClientMessage: //TODO: We aren't reading this correctly, It could be anything, highest chance is that it's a close event though
	    Video_Shutdown(); // Calling from here since returning false does nothing
	    return; 
	    break;
	default:
	    //TODO: Should we put the event back if we don't handle it?
	    // I think we handle all the needed ones, the rest shouldn't matter
	    // But to be safe, let's but them back anyway
	    //XPutBackEvent(dpy, &event);
	    break;
	}
    }

    float FactorW  = 640.0f / (float)GetWidth();
    float FactorH  = 480.0f / (float)GetHeight();
    float Max = (FactorW < FactorH) ? FactorH : FactorW;
    //    AR = (float)surface->w / (float)surface->h;;
    
    if (g_Config.bStretchToFit) {
	SetMax(1,1);
	SetOffset(0,0);
    } else {
	SetMax(1.0f / Max, 1.0f / Max);
	SetOffset((int)((GetWidth() - (640 * GetXmax())) / 2),
		  (int)((GetHeight() - (480 * GetYmax())) / 2));
    }
    
}

bool X11Window::MakeCurrent() {

    Window winDummy;
    unsigned int borderDummy;
    int x,y;
    u32 w,h,depth;
    // connect the glx-context to the window
    glXMakeCurrent(dpy, win, ctx);
    XGetGeometry(dpy, win, &winDummy, &x, &y,
                 &w, &h, &borderDummy, &depth);
    SetSize(w, h);
    ERROR_LOG("GLWin Depth %d", depth);
    if (glXIsDirect(dpy, ctx)) 
        ERROR_LOG("you have Direct Rendering!");
    else
        ERROR_LOG("no Direct Rendering possible!");

    // better for pad plugin key input (thc)
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | 
		 KeyReleaseMask | ButtonReleaseMask | StructureNotifyMask | 
		 EnterWindowMask | LeaveWindowMask | FocusChangeMask);
    
    return true;
}
