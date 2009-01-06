#include "X11Window.h"

 static EventHandler *eventHandler = EventHandler::GetInstance();

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

// Taken from sfml code
void X11Window::ProcessEvent(XEvent WinEvent) {
    //    static EventHandler *eventHandler = EventHandler::GetInstance();
    switch (WinEvent.type) {
	
    case KeyPress :
	{
	// Get the keysym of the key that has been pressed
	static XComposeStatus KeyboardStatus;
	char Buffer[32];
	KeySym Sym;
	XLookupString(&WinEvent.xkey, Buffer, sizeof(Buffer), &Sym, &KeyboardStatus);
	
	// Fill the event parameters
	sf::Event Evt;
	Evt.Type        = sf::Event::KeyPressed;
	Evt.Key.Code    = KeysymToSF(Sym);
	Evt.Key.Alt     = WinEvent.xkey.state & Mod1Mask;
	Evt.Key.Control = WinEvent.xkey.state & ControlMask;
	Evt.Key.Shift   = WinEvent.xkey.state & ShiftMask;
	eventHandler->addEvent(&Evt);
	break;
	}
	// Key up event
    case KeyRelease :
	{
	// Get the keysym of the key that has been pressed
	char Buffer[32];
	KeySym Sym;
	XLookupString(&WinEvent.xkey, Buffer, 32, &Sym, NULL);
	
	// Fill the event parameters
	sf::Event Evt;
	Evt.Type        = sf::Event::KeyReleased;
	Evt.Key.Code    = KeysymToSF(Sym);
	Evt.Key.Alt     = WinEvent.xkey.state & Mod1Mask;
	Evt.Key.Control = WinEvent.xkey.state & ControlMask;
	Evt.Key.Shift   = WinEvent.xkey.state & ShiftMask;
	eventHandler->addEvent(&Evt);
	break;
	}
    }
}
 
void X11Window::Update() {
    // We just check all of our events here
    XEvent event;
    //    KeySym key;
    //    int x,y;
    //    u32 w,h,depth;

    int num_events;
    for (num_events = XPending(dpy);num_events > 0;num_events--) {
	XNextEvent(dpy, &event);
	ProcessEvent(event);
	/*	case ButtonPress:
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
	    }*/
    }

    eventHandler->Update();

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

// Taken from SF code
sf::Key::Code X11Window::KeysymToSF(KeySym Sym) {
    // First convert to uppercase (to avoid dealing with two different keysyms for the same key)
    KeySym Lower, Key;
    XConvertCase(Sym, &Lower, &Key);
    
    switch (Key)
	{
        case XK_Shift_L :      return sf::Key::LShift;
        case XK_Shift_R :      return sf::Key::RShift;
        case XK_Control_L :    return sf::Key::LControl;
        case XK_Control_R :    return sf::Key::RControl;
        case XK_Alt_L :        return sf::Key::LAlt;
        case XK_Alt_R :        return sf::Key::RAlt;
        case XK_Super_L :      return sf::Key::LSystem;
        case XK_Super_R :      return sf::Key::RSystem;
        case XK_Menu :         return sf::Key::Menu;
        case XK_Escape :       return sf::Key::Escape;
        case XK_semicolon :    return sf::Key::SemiColon;
        case XK_slash :        return sf::Key::Slash;
        case XK_equal :        return sf::Key::Equal;
        case XK_minus :        return sf::Key::Dash;
        case XK_bracketleft :  return sf::Key::LBracket;
        case XK_bracketright : return sf::Key::RBracket;
        case XK_comma :        return sf::Key::Comma;
        case XK_period :       return sf::Key::Period;
        case XK_dead_acute :   return sf::Key::Quote;
        case XK_backslash :    return sf::Key::BackSlash;
        case XK_dead_grave :   return sf::Key::Tilde;
        case XK_space :        return sf::Key::Space;
        case XK_Return :       return sf::Key::Return;
        case XK_KP_Enter :     return sf::Key::Return;
        case XK_BackSpace :    return sf::Key::Back;
        case XK_Tab :          return sf::Key::Tab;
        case XK_Prior :        return sf::Key::PageUp;
        case XK_Next :         return sf::Key::PageDown;
        case XK_End :          return sf::Key::End;
        case XK_Home :         return sf::Key::Home;
        case XK_Insert :       return sf::Key::Insert;
        case XK_Delete :       return sf::Key::Delete;
        case XK_KP_Add :       return sf::Key::Add;
        case XK_KP_Subtract :  return sf::Key::Subtract;
        case XK_KP_Multiply :  return sf::Key::Multiply;
        case XK_KP_Divide :    return sf::Key::Divide;
        case XK_Pause :        return sf::Key::Pause;
        case XK_F1 :           return sf::Key::F1;
        case XK_F2 :           return sf::Key::F2;
        case XK_F3 :           return sf::Key::F3;
        case XK_F4 :           return sf::Key::F4;
        case XK_F5 :           return sf::Key::F5;
        case XK_F6 :           return sf::Key::F6;
        case XK_F7 :           return sf::Key::F7;
        case XK_F8 :           return sf::Key::F8;
        case XK_F9 :           return sf::Key::F9;
        case XK_F10 :          return sf::Key::F10;
        case XK_F11 :          return sf::Key::F11;
        case XK_F12 :          return sf::Key::F12;
        case XK_F13 :          return sf::Key::F13;
        case XK_F14 :          return sf::Key::F14;
        case XK_F15 :          return sf::Key::F15;
        case XK_Left :         return sf::Key::Left;
        case XK_Right :        return sf::Key::Right;
        case XK_Up :           return sf::Key::Up;
        case XK_Down :         return sf::Key::Down;
        case XK_KP_0 :         return sf::Key::Numpad0;
        case XK_KP_1 :         return sf::Key::Numpad1;
        case XK_KP_2 :         return sf::Key::Numpad2;
        case XK_KP_3 :         return sf::Key::Numpad3;
        case XK_KP_4 :         return sf::Key::Numpad4;
        case XK_KP_5 :         return sf::Key::Numpad5;
        case XK_KP_6 :         return sf::Key::Numpad6;
        case XK_KP_7 :         return sf::Key::Numpad7;
        case XK_KP_8 :         return sf::Key::Numpad8;
	case XK_Z :            return sf::Key::Z;
        case XK_E :            return sf::Key::E;
        case XK_R :            return sf::Key::R;
        case XK_T :            return sf::Key::T;
        case XK_Y :            return sf::Key::Y;
        case XK_U :            return sf::Key::U;
        case XK_I :            return sf::Key::I;
        case XK_O :            return sf::Key::O;
        case XK_P :            return sf::Key::P;
        case XK_Q :            return sf::Key::Q;
        case XK_S :            return sf::Key::S;
        case XK_D :            return sf::Key::D;
        case XK_F :            return sf::Key::F;
        case XK_G :            return sf::Key::G;
        case XK_H :            return sf::Key::H;
        case XK_J :            return sf::Key::J;
        case XK_K :            return sf::Key::K;
        case XK_L :            return sf::Key::L;
        case XK_M :            return sf::Key::M;
        case XK_W :            return sf::Key::W;
        case XK_X :            return sf::Key::X;
        case XK_C :            return sf::Key::C;
        case XK_V :            return sf::Key::V;
        case XK_B :            return sf::Key::B;
        case XK_N :            return sf::Key::N;
        case XK_0 :            return sf::Key::Num0;
        case XK_1 :            return sf::Key::Num1;
        case XK_2 :            return sf::Key::Num2;
        case XK_3 :            return sf::Key::Num3;
        case XK_4 :            return sf::Key::Num4;
        case XK_5 :            return sf::Key::Num5;
        case XK_6 :            return sf::Key::Num6;
        case XK_7 :            return sf::Key::Num7;
        case XK_8 :            return sf::Key::Num8;
        case XK_9 :            return sf::Key::Num9;
	}
    return sf::Key::Code(0);
}
