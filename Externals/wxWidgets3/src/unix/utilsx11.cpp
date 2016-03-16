/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/utilsx11.cpp
// Purpose:     Miscellaneous X11 functions (for wxCore)
// Author:      Mattia Barbon, Vaclav Slavik, Robert Roebling
// Modified by:
// Created:     25.03.02
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__WXX11__) || defined(__WXGTK__) || defined(__WXMOTIF__)

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/unix/utilsx11.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/icon.h"
    #include "wx/image.h"
#endif

#include "wx/iconbndl.h"
#include "wx/apptrait.h"
#include "wx/private/launchbrowser.h"

#ifdef __VMS
#pragma message disable nosimpint
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef __VMS
#pragma message enable nosimpint
#endif

#ifdef __WXGTK__
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#ifdef __WXGTK20__
#include "wx/gtk/private/gtk2-compat.h"     // gdk_window_get_screen()
#endif
#endif
GdkWindow* wxGetTopLevelGDK();
#endif

// Only X11 backend is supported for wxGTK here
#if !defined(__WXGTK__) || defined(GDK_WINDOWING_X11)

// Various X11 Atoms used in this file:
static Atom _NET_WM_STATE = 0;
static Atom _NET_WM_STATE_FULLSCREEN = 0;
static Atom _NET_WM_STATE_STAYS_ON_TOP = 0;
static Atom _NET_WM_WINDOW_TYPE = 0;
static Atom _NET_WM_WINDOW_TYPE_NORMAL = 0;
static Atom _KDE_NET_WM_WINDOW_TYPE_OVERRIDE = 0;
static Atom _WIN_LAYER = 0;
static Atom KWIN_RUNNING = 0;
#ifndef __WXGTK20__
static Atom _NET_SUPPORTING_WM_CHECK = 0;
static Atom _NET_SUPPORTED = 0;
#endif

#define wxMAKE_ATOM(name, display) \
    if (name == 0) name = XInternAtom((display), #name, False)


// X11 Window is an int type, so use the macro to suppress warnings when
// converting to it
#define WindowCast(w) (Window)(wxPtrToUInt(w))

// Is the window mapped?
static bool IsMapped(Display *display, Window window)
{
    XWindowAttributes attr;
    XGetWindowAttributes(display, window, &attr);
    return (attr.map_state != IsUnmapped);
}



// Suspends X11 errors. Used when we expect errors but they are not fatal
// for us.
extern "C"
{
    typedef int (*wxX11ErrorHandler)(Display *, XErrorEvent *);

    static int wxX11ErrorsSuspender_handler(Display*, XErrorEvent*) { return 0; }
}

class wxX11ErrorsSuspender
{
public:
    wxX11ErrorsSuspender(Display *d) : m_display(d)
    {
        m_old = XSetErrorHandler(wxX11ErrorsSuspender_handler);
    }
    ~wxX11ErrorsSuspender()
    {
        XFlush(m_display);
        XSetErrorHandler(m_old);
    }

private:
    Display *m_display;
    wxX11ErrorHandler m_old;
};



// ----------------------------------------------------------------------------
// Setting icons for window manager:
// ----------------------------------------------------------------------------

#if wxUSE_IMAGE && !wxUSE_NANOX

static Atom _NET_WM_ICON = 0;

void
wxSetIconsX11(WXDisplay* display, WXWindow window, const wxIconBundle& ib)
{
    size_t size = 0;

    const size_t numIcons = ib.GetIconCount();
    for ( size_t i = 0; i < numIcons; ++i )
    {
        const wxIcon icon = ib.GetIconByIndex(i);

        size += 2 + icon.GetWidth() * icon.GetHeight();
    }

    wxMAKE_ATOM(_NET_WM_ICON, (Display*)display);

    if ( size > 0 )
    {
        unsigned long* data = new unsigned long[size];
        unsigned long* ptr = data;

        for ( size_t i = 0; i < numIcons; ++i )
        {
            const wxImage image = ib.GetIconByIndex(i).ConvertToImage();
            int width = image.GetWidth(),
                height = image.GetHeight();
            unsigned char* imageData = image.GetData();
            unsigned char* imageDataEnd = imageData + ( width * height * 3 );
            bool hasMask = image.HasMask();
            unsigned char rMask, gMask, bMask;
            unsigned char r, g, b, a;

            if( hasMask )
            {
                rMask = image.GetMaskRed();
                gMask = image.GetMaskGreen();
                bMask = image.GetMaskBlue();
            }
            else // no mask, but still init the variables to avoid warnings
            {
                rMask =
                gMask =
                bMask = 0;
            }

            *ptr++ = width;
            *ptr++ = height;

            while ( imageData < imageDataEnd )
            {
                r = imageData[0];
                g = imageData[1];
                b = imageData[2];
                if( hasMask && r == rMask && g == gMask && b == bMask )
                    a = 0;
                else
                    a = 255;

                *ptr++ = ( a << 24 ) | ( r << 16 ) | ( g << 8 ) | b;

                imageData += 3;
            }
        }

        XChangeProperty( (Display*)display,
                         WindowCast(window),
                         _NET_WM_ICON,
                         XA_CARDINAL, 32,
                         PropModeReplace,
                         (unsigned char*)data, size );
        delete[] data;
    }
    else
    {
        XDeleteProperty( (Display*)display,
                         WindowCast(window),
                         _NET_WM_ICON );
    }
}

#endif // wxUSE_IMAGE && !wxUSE_NANOX

// ----------------------------------------------------------------------------
// Fullscreen mode:
// ----------------------------------------------------------------------------

// NB: Setting fullscreen mode under X11 is a complicated matter. There was
//     no standard way of doing it until recently. ICCCM doesn't know the
//     concept of fullscreen windows and the only way to make a window
//     fullscreen is to remove decorations, resize it to cover entire screen
//     and set WIN_LAYER_ABOVE_DOCK.
//
//     This doesn't always work, though. Specifically, at least kwin from
//     KDE 3 ignores the hint. The only way to make kwin accept our request
//     is to emulate the way Qt does it. That is, unmap the window, set
//     _NET_WM_WINDOW_TYPE to _KDE_NET_WM_WINDOW_TYPE_OVERRIDE (KDE extension),
//     add _NET_WM_STATE_STAYS_ON_TOP (ditto) to _NET_WM_STATE and map
//     the window again.
//
//     Version 1.2 of Window Manager Specification (aka wm-spec aka
//     Extended Window Manager Hints) introduced _NET_WM_STATE_FULLSCREEN
//     window state which provides cleanest and simplest possible way of
//     making a window fullscreen. WM-spec is a de-facto standard adopted
//     by GNOME and KDE folks, but _NET_WM_STATE_FULLSCREEN isn't yet widely
//     supported. As of January 2003, only GNOME 2's default WM Metacity
//     implements, KDE will support it from version 3.2. At toolkits level,
//     GTK+ >= 2.1.2 uses it as the only method of making windows fullscreen
//     (that's why wxGTK will *not* switch to using gtk_window_fullscreen
//     unless it has better compatibility with older WMs).
//
//
//     This is what wxWidgets does in wxSetFullScreenStateX11:
//       1) if _NET_WM_STATE_FULLSCREEN is supported, use it
//       2) otherwise try WM-specific hacks (KDE, IceWM)
//       3) use _WIN_LAYER and hope that the WM will recognize it
//     The code was tested with:
//     twm, IceWM, WindowMaker, Metacity, kwin, sawfish, lesstif-mwm


#define  WIN_LAYER_NORMAL       4
#define  WIN_LAYER_ABOVE_DOCK  10

static void wxWinHintsSetLayer(Display *display, Window rootWnd,
                               Window window, int layer)
{
    wxX11ErrorsSuspender noerrors(display);

    XEvent xev;

    wxMAKE_ATOM( _WIN_LAYER, display );

    if (IsMapped(display, window))
    {
        xev.type = ClientMessage;
        xev.xclient.type = ClientMessage;
        xev.xclient.window = window;
        xev.xclient.message_type = _WIN_LAYER;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = (long)layer;
        xev.xclient.data.l[1] = CurrentTime;

        XSendEvent(display, rootWnd, False,
                   SubstructureNotifyMask, (XEvent*) &xev);
    }
    else
    {
        long data[1];

        data[0] = layer;
        XChangeProperty(display, window,
                        _WIN_LAYER, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)data, 1);
    }
}



#ifdef __WXGTK20__
static bool wxQueryWMspecSupport(Display* WXUNUSED(display),
                                 Window WXUNUSED(rootWnd),
                                 Atom (feature))
{
    GdkAtom gatom = gdk_x11_xatom_to_atom(feature);
    return gdk_x11_screen_supports_net_wm_hint(gdk_screen_get_default(), gatom);
}
#else
static bool wxQueryWMspecSupport(Display *display, Window rootWnd, Atom feature)
{
    wxMAKE_ATOM(_NET_SUPPORTING_WM_CHECK, display);
    wxMAKE_ATOM(_NET_SUPPORTED, display);

    // FIXME: We may want to cache these checks. Note that we can't simply
    //        remember the results in global variable because the WM may go
    //        away and be replaced by another one! One possible approach
    //        would be invalidate the case every 15 seconds or so. Since this
    //        code is currently only used by wxTopLevelWindow::ShowFullScreen,
    //        it is not important that it is not optimized.
    //
    //        If the WM supports ICCCM (i.e. the root window has
    //        _NET_SUPPORTING_WM_CHECK property that points to a WM-owned
    //        window), we could watch for DestroyNotify event on the window
    //        and invalidate our cache when the windows goes away (= WM
    //        is replaced by another one). This is what GTK+ 2 does.
    //        Let's do it only if it is needed, it requires changes to
    //        the event loop.

    Atom type;
    Window *wins;
    Atom *atoms;
    int format;
    unsigned long after;
    unsigned long nwins, natoms;

    // Is the WM ICCCM supporting?
    XGetWindowProperty(display, rootWnd,
                       _NET_SUPPORTING_WM_CHECK, 0, LONG_MAX,
                       False, XA_WINDOW, &type, &format, &nwins,
                       &after, (unsigned char **)&wins);
    if ( type != XA_WINDOW || nwins == 0 || wins[0] == None )
       return false;
    XFree(wins);

    // Query for supported features:
    XGetWindowProperty(display, rootWnd,
                       _NET_SUPPORTED, 0, LONG_MAX,
                       False, XA_ATOM, &type, &format, &natoms,
                       &after, (unsigned char **)&atoms);
    if ( type != XA_ATOM || atoms == NULL )
        return false;

    // Lookup the feature we want:
    for (unsigned i = 0; i < natoms; i++)
    {
        if ( atoms[i] == feature )
        {
            XFree(atoms);
            return true;
        }
    }
    XFree(atoms);
    return false;
}
#endif


#define _NET_WM_STATE_REMOVE        0
#define _NET_WM_STATE_ADD           1

static void wxWMspecSetState(Display *display, Window rootWnd,
                             Window window, int operation, Atom state)
{
    wxMAKE_ATOM(_NET_WM_STATE, display);

    if ( IsMapped(display, window) )
    {
        XEvent xev;
        xev.type = ClientMessage;
        xev.xclient.type = ClientMessage;
        xev.xclient.serial = 0;
        xev.xclient.send_event = True;
        xev.xclient.display = display;
        xev.xclient.window = window;
        xev.xclient.message_type = _NET_WM_STATE;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = operation;
        xev.xclient.data.l[1] = state;
        xev.xclient.data.l[2] = None;

        XSendEvent(display, rootWnd,
                   False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &xev);
    }
    // FIXME - must modify _NET_WM_STATE property list if the window
    //         wasn't mapped!
}

static void wxWMspecSetFullscreen(Display *display, Window rootWnd,
                                  Window window, bool fullscreen)
{
    wxMAKE_ATOM(_NET_WM_STATE_FULLSCREEN, display);
    wxWMspecSetState(display, rootWnd,
                     window,
                     fullscreen ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
                      _NET_WM_STATE_FULLSCREEN);
}


// Is the user running KDE's kwin window manager? At least kwin from KDE 3
// sets KWIN_RUNNING property on the root window.
static bool wxKwinRunning(Display *display, Window rootWnd)
{
    wxMAKE_ATOM(KWIN_RUNNING, display);

    unsigned char* data;
    Atom type;
    int format;
    unsigned long nitems, after;
    if (XGetWindowProperty(display, rootWnd,
                           KWIN_RUNNING, 0, 1, False, KWIN_RUNNING,
                           &type, &format, &nitems, &after,
                           &data) != Success)
    {
        return false;
    }

    bool retval = (type == KWIN_RUNNING &&
                   nitems == 1 && data && ((long*)data)[0] == 1);
    XFree(data);
    return retval;
}

// KDE's kwin is Qt-centric so much than no normal method of fullscreen
// mode will work with it. We have to carefully emulate the Qt way.
static void wxSetKDEFullscreen(Display *display, Window rootWnd,
                               Window w, bool fullscreen, wxRect *origRect)
{
    long data[2];
    unsigned lng;

    wxMAKE_ATOM(_NET_WM_WINDOW_TYPE, display);
    wxMAKE_ATOM(_NET_WM_WINDOW_TYPE_NORMAL, display);
    wxMAKE_ATOM(_KDE_NET_WM_WINDOW_TYPE_OVERRIDE, display);
    wxMAKE_ATOM(_NET_WM_STATE_STAYS_ON_TOP, display);

    if (fullscreen)
    {
        data[0] = _KDE_NET_WM_WINDOW_TYPE_OVERRIDE;
        data[1] = _NET_WM_WINDOW_TYPE_NORMAL;
        lng = 2;
    }
    else
    {
        data[0] = _NET_WM_WINDOW_TYPE_NORMAL;
        data[1] = None;
        lng = 1;
    }

    // it is necessary to unmap the window, otherwise kwin will ignore us:
    XSync(display, False);

    bool wasMapped = IsMapped(display, w);
    if (wasMapped)
    {
        XUnmapWindow(display, w);
        XSync(display, False);
    }

    XChangeProperty(display, w, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) &data[0], lng);
    XSync(display, False);

    if (wasMapped)
    {
        XMapRaised(display, w);
        XSync(display, False);
    }

    wxWMspecSetState(display, rootWnd, w,
                     fullscreen ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
                     _NET_WM_STATE_STAYS_ON_TOP);
    XSync(display, False);

    if (!fullscreen)
    {
        // NB: like many other WMs, kwin ignores the first request for a window
        //     position change after the window was mapped. This additional
        //     move+resize event will ensure that the window is restored in
        //     exactly the same position as before it was made fullscreen
        //     (because wxTopLevelWindow::ShowFullScreen will call SetSize, thus
        //     setting the position for the second time).
        XMoveResizeWindow(display, w,
                          origRect->x, origRect->y,
                          origRect->width, origRect->height);
        XSync(display, False);
    }
}


wxX11FullScreenMethod wxGetFullScreenMethodX11(WXDisplay* display,
                                               WXWindow rootWindow)
{
    Window root = WindowCast(rootWindow);
    Display *disp = (Display*)display;

    // if WM supports _NET_WM_STATE_FULLSCREEN from wm-spec 1.2, use it:
    wxMAKE_ATOM(_NET_WM_STATE_FULLSCREEN, disp);
    if (wxQueryWMspecSupport(disp, root, _NET_WM_STATE_FULLSCREEN))
    {
        wxLogTrace(wxT("fullscreen"),
                   wxT("detected _NET_WM_STATE_FULLSCREEN support"));
        return wxX11_FS_WMSPEC;
    }

    // if the user is running KDE's kwin WM, use a legacy hack because
    // kwin doesn't understand any other method:
    if (wxKwinRunning(disp, root))
    {
        wxLogTrace(wxT("fullscreen"), wxT("detected kwin"));
        return wxX11_FS_KDE;
    }

    // finally, fall back to ICCCM heuristic method:
    wxLogTrace(wxT("fullscreen"), wxT("unknown WM, using _WIN_LAYER"));
    return wxX11_FS_GENERIC;
}


void wxSetFullScreenStateX11(WXDisplay* display, WXWindow rootWindow,
                             WXWindow window, bool show,
                             wxRect *origRect,
                             wxX11FullScreenMethod method)
{
    // NB: please see the comment under "Fullscreen mode:" title above
    //     for implications of changing this code.

    Window wnd = WindowCast(window);
    Window root = WindowCast(rootWindow);
    Display *disp = (Display*)display;

    if (method == wxX11_FS_AUTODETECT)
        method = wxGetFullScreenMethodX11(display, rootWindow);

    switch (method)
    {
        case wxX11_FS_WMSPEC:
            wxWMspecSetFullscreen(disp, root, wnd, show);
            break;
        case wxX11_FS_KDE:
            wxSetKDEFullscreen(disp, root, wnd, show, origRect);
            break;
        default:
            wxWinHintsSetLayer(disp, root, wnd,
                               show ? WIN_LAYER_ABOVE_DOCK : WIN_LAYER_NORMAL);
            break;
    }
}



// ----------------------------------------------------------------------------
// keycode translations
// ----------------------------------------------------------------------------

#include <X11/keysym.h>

// FIXME what about tables??

int wxCharCodeXToWX(WXKeySym keySym)
{
    int id;
    switch (keySym)
    {
        case XK_Shift_L:
        case XK_Shift_R:
            id = WXK_SHIFT; break;
        case XK_Control_L:
        case XK_Control_R:
            id = WXK_CONTROL; break;
        case XK_Meta_L:
        case XK_Meta_R:
            id = WXK_ALT; break;
        case XK_Caps_Lock:
            id = WXK_CAPITAL; break;
        case XK_BackSpace:
            id = WXK_BACK; break;
        case XK_Delete:
            id = WXK_DELETE; break;
        case XK_Clear:
            id = WXK_CLEAR; break;
        case XK_Tab:
            id = WXK_TAB; break;
        case XK_numbersign:
            id = '#'; break;
        case XK_Return:
            id = WXK_RETURN; break;
        case XK_Escape:
            id = WXK_ESCAPE; break;
        case XK_Pause:
        case XK_Break:
            id = WXK_PAUSE; break;
        case XK_Num_Lock:
            id = WXK_NUMLOCK; break;
        case XK_Scroll_Lock:
            id = WXK_SCROLL; break;

        case XK_Home:
            id = WXK_HOME; break;
        case XK_End:
            id = WXK_END; break;
        case XK_Left:
            id = WXK_LEFT; break;
        case XK_Right:
            id = WXK_RIGHT; break;
        case XK_Up:
            id = WXK_UP; break;
        case XK_Down:
            id = WXK_DOWN; break;
        case XK_Next:
            id = WXK_PAGEDOWN; break;
        case XK_Prior:
            id = WXK_PAGEUP; break;
        case XK_Menu:
            id = WXK_MENU; break;
        case XK_Select:
            id = WXK_SELECT; break;
        case XK_Cancel:
            id = WXK_CANCEL; break;
        case XK_Print:
            id = WXK_PRINT; break;
        case XK_Execute:
            id = WXK_EXECUTE; break;
        case XK_Insert:
            id = WXK_INSERT; break;
        case XK_Help:
            id = WXK_HELP; break;

        case XK_KP_Multiply:
            id = WXK_NUMPAD_MULTIPLY; break;
        case XK_KP_Add:
            id = WXK_NUMPAD_ADD; break;
        case XK_KP_Subtract:
            id = WXK_NUMPAD_SUBTRACT; break;
        case XK_KP_Divide:
            id = WXK_NUMPAD_DIVIDE; break;
        case XK_KP_Decimal:
            id = WXK_NUMPAD_DECIMAL; break;
        case XK_KP_Equal:
            id = WXK_NUMPAD_EQUAL; break;
        case XK_KP_Space:
            id = WXK_NUMPAD_SPACE; break;
        case XK_KP_Tab:
            id = WXK_NUMPAD_TAB; break;
        case XK_KP_Enter:
            id = WXK_NUMPAD_ENTER; break;
        case XK_KP_0:
            id = WXK_NUMPAD0; break;
        case XK_KP_1:
            id = WXK_NUMPAD1; break;
        case XK_KP_2:
            id = WXK_NUMPAD2; break;
        case XK_KP_3:
            id = WXK_NUMPAD3; break;
        case XK_KP_4:
            id = WXK_NUMPAD4; break;
        case XK_KP_5:
            id = WXK_NUMPAD5; break;
        case XK_KP_6:
            id = WXK_NUMPAD6; break;
        case XK_KP_7:
            id = WXK_NUMPAD7; break;
        case XK_KP_8:
            id = WXK_NUMPAD8; break;
        case XK_KP_9:
            id = WXK_NUMPAD9; break;
        case XK_KP_Insert:
            id = WXK_NUMPAD_INSERT; break;
        case XK_KP_End:
            id = WXK_NUMPAD_END; break;
        case XK_KP_Down:
            id = WXK_NUMPAD_DOWN; break;
        case XK_KP_Page_Down:
            id = WXK_NUMPAD_PAGEDOWN; break;
        case XK_KP_Left:
            id = WXK_NUMPAD_LEFT; break;
        case XK_KP_Right:
            id = WXK_NUMPAD_RIGHT; break;
        case XK_KP_Home:
            id = WXK_NUMPAD_HOME; break;
        case XK_KP_Up:
            id = WXK_NUMPAD_UP; break;
        case XK_KP_Page_Up:
            id = WXK_NUMPAD_PAGEUP; break;
        case XK_F1:
            id = WXK_F1; break;
        case XK_F2:
            id = WXK_F2; break;
        case XK_F3:
            id = WXK_F3; break;
        case XK_F4:
            id = WXK_F4; break;
        case XK_F5:
            id = WXK_F5; break;
        case XK_F6:
            id = WXK_F6; break;
        case XK_F7:
            id = WXK_F7; break;
        case XK_F8:
            id = WXK_F8; break;
        case XK_F9:
            id = WXK_F9; break;
        case XK_F10:
            id = WXK_F10; break;
        case XK_F11:
            id = WXK_F11; break;
        case XK_F12:
            id = WXK_F12; break;
        case XK_F13:
            id = WXK_F13; break;
        case XK_F14:
            id = WXK_F14; break;
        case XK_F15:
            id = WXK_F15; break;
        case XK_F16:
            id = WXK_F16; break;
        case XK_F17:
            id = WXK_F17; break;
        case XK_F18:
            id = WXK_F18; break;
        case XK_F19:
            id = WXK_F19; break;
        case XK_F20:
            id = WXK_F20; break;
        case XK_F21:
            id = WXK_F21; break;
        case XK_F22:
            id = WXK_F22; break;
        case XK_F23:
            id = WXK_F23; break;
        case XK_F24:
            id = WXK_F24; break;
        default:
            id = (keySym <= 255) ? (int)keySym : -1;
    }

    return id;
}

WXKeySym wxCharCodeWXToX(int id)
{
    WXKeySym keySym;

    switch (id)
    {
        case WXK_CANCEL:        keySym = XK_Cancel; break;
        case WXK_BACK:          keySym = XK_BackSpace; break;
        case WXK_TAB:           keySym = XK_Tab; break;
        case WXK_CLEAR:         keySym = XK_Clear; break;
        case WXK_RETURN:        keySym = XK_Return; break;
        case WXK_SHIFT:         keySym = XK_Shift_L; break;
        case WXK_CONTROL:       keySym = XK_Control_L; break;
        case WXK_ALT:           keySym = XK_Meta_L; break;
        case WXK_CAPITAL:       keySym = XK_Caps_Lock; break;
        case WXK_MENU :         keySym = XK_Menu; break;
        case WXK_PAUSE:         keySym = XK_Pause; break;
        case WXK_ESCAPE:        keySym = XK_Escape; break;
        case WXK_SPACE:         keySym = ' '; break;
        case WXK_PAGEUP:        keySym = XK_Prior; break;
        case WXK_PAGEDOWN:      keySym = XK_Next; break;
        case WXK_END:           keySym = XK_End; break;
        case WXK_HOME :         keySym = XK_Home; break;
        case WXK_LEFT :         keySym = XK_Left; break;
        case WXK_UP:            keySym = XK_Up; break;
        case WXK_RIGHT:         keySym = XK_Right; break;
        case WXK_DOWN :         keySym = XK_Down; break;
        case WXK_SELECT:        keySym = XK_Select; break;
        case WXK_PRINT:         keySym = XK_Print; break;
        case WXK_EXECUTE:       keySym = XK_Execute; break;
        case WXK_INSERT:        keySym = XK_Insert; break;
        case WXK_DELETE:        keySym = XK_Delete; break;
        case WXK_HELP :         keySym = XK_Help; break;
        case WXK_NUMPAD0:       keySym = XK_KP_0; break; case WXK_NUMPAD_INSERT:     keySym = XK_KP_Insert; break;
        case WXK_NUMPAD1:       keySym = XK_KP_1; break; case WXK_NUMPAD_END:           keySym = XK_KP_End; break;
        case WXK_NUMPAD2:       keySym = XK_KP_2; break; case WXK_NUMPAD_DOWN:      keySym = XK_KP_Down; break;
        case WXK_NUMPAD3:       keySym = XK_KP_3; break; case WXK_NUMPAD_PAGEDOWN:  keySym = XK_KP_Page_Down; break;
        case WXK_NUMPAD4:       keySym = XK_KP_4; break; case WXK_NUMPAD_LEFT:         keySym = XK_KP_Left; break;
        case WXK_NUMPAD5:       keySym = XK_KP_5; break;
        case WXK_NUMPAD6:       keySym = XK_KP_6; break; case WXK_NUMPAD_RIGHT:       keySym = XK_KP_Right; break;
        case WXK_NUMPAD7:       keySym = XK_KP_7; break; case WXK_NUMPAD_HOME:       keySym = XK_KP_Home; break;
        case WXK_NUMPAD8:       keySym = XK_KP_8; break; case WXK_NUMPAD_UP:             keySym = XK_KP_Up; break;
        case WXK_NUMPAD9:       keySym = XK_KP_9; break; case WXK_NUMPAD_PAGEUP:   keySym = XK_KP_Page_Up; break;
        case WXK_NUMPAD_DECIMAL:    keySym = XK_KP_Decimal; break; case WXK_NUMPAD_DELETE:   keySym = XK_KP_Delete; break;
        case WXK_NUMPAD_MULTIPLY:   keySym = XK_KP_Multiply; break;
        case WXK_NUMPAD_ADD:             keySym = XK_KP_Add; break;
        case WXK_NUMPAD_SUBTRACT: keySym = XK_KP_Subtract; break;
        case WXK_NUMPAD_DIVIDE:        keySym = XK_KP_Divide; break;
        case WXK_NUMPAD_ENTER:        keySym = XK_KP_Enter; break;
        case WXK_NUMPAD_SEPARATOR:  keySym = XK_KP_Separator; break;
        case WXK_F1:            keySym = XK_F1; break;
        case WXK_F2:            keySym = XK_F2; break;
        case WXK_F3:            keySym = XK_F3; break;
        case WXK_F4:            keySym = XK_F4; break;
        case WXK_F5:            keySym = XK_F5; break;
        case WXK_F6:            keySym = XK_F6; break;
        case WXK_F7:            keySym = XK_F7; break;
        case WXK_F8:            keySym = XK_F8; break;
        case WXK_F9:            keySym = XK_F9; break;
        case WXK_F10:           keySym = XK_F10; break;
        case WXK_F11:           keySym = XK_F11; break;
        case WXK_F12:           keySym = XK_F12; break;
        case WXK_F13:           keySym = XK_F13; break;
        case WXK_F14:           keySym = XK_F14; break;
        case WXK_F15:           keySym = XK_F15; break;
        case WXK_F16:           keySym = XK_F16; break;
        case WXK_F17:           keySym = XK_F17; break;
        case WXK_F18:           keySym = XK_F18; break;
        case WXK_F19:           keySym = XK_F19; break;
        case WXK_F20:           keySym = XK_F20; break;
        case WXK_F21:           keySym = XK_F21; break;
        case WXK_F22:           keySym = XK_F22; break;
        case WXK_F23:           keySym = XK_F23; break;
        case WXK_F24:           keySym = XK_F24; break;
        case WXK_NUMLOCK:       keySym = XK_Num_Lock; break;
        case WXK_SCROLL:        keySym = XK_Scroll_Lock; break;
        default:                keySym = id <= 255 ? (KeySym)id : 0;
    }

    return keySym;
}

// all Xlib keysym-unicode charactor pair that could
// type from keyboard directly under variety layout
// Credits: This tab just modified around a public domain keysym dataset
// write by Markus G. Kuhn. See the link below.
// http://www.cl.cam.ac.uk/~mgk25/ucs/keysyms.txt
CodePair keySymTab[] = {
    {0x0020, 0x0020},    // space
    {0x0021, 0x0021},    // exclam
    {0x0022, 0x0022},    // quotedbl
    {0x0023, 0x0023},    // numbersign
    {0x0024, 0x0024},    // dollar
    {0x0025, 0x0025},    // percent
    {0x0026, 0x0026},    // ampersand
    {0x0027, 0x0027},    // apostrophe
    {0x0027, 0x0027},    // quoteright /* deprecated */
    {0x0028, 0x0028},    // parenleft
    {0x0029, 0x0029},    // parenright
    {0x002a, 0x002a},    // asterisk
    {0x002b, 0x002b},    // plus
    {0x002c, 0x002c},    // comma
    {0x002d, 0x002d},    // minus
    {0x002e, 0x002e},    // period
    {0x002f, 0x002f},    // slash
    {0x0030, 0x0030},    // 0
    {0x0031, 0x0031},    // 1
    {0x0032, 0x0032},    // 2
    {0x0033, 0x0033},    // 3
    {0x0034, 0x0034},    // 4
    {0x0035, 0x0035},    // 5
    {0x0036, 0x0036},    // 6
    {0x0037, 0x0037},    // 7
    {0x0038, 0x0038},    // 8
    {0x0039, 0x0039},    // 9
    {0x003a, 0x003a},    // colon
    {0x003b, 0x003b},    // semicolon
    {0x003c, 0x003c},    // less
    {0x003d, 0x003d},    // equal
    {0x003e, 0x003e},    // greater
    {0x003f, 0x003f},    // question
    {0x0040, 0x0040},    // at
    {0x0041, 0x0041},    // A
    {0x0042, 0x0042},    // B
    {0x0043, 0x0043},    // C
    {0x0044, 0x0044},    // D
    {0x0045, 0x0045},    // E
    {0x0046, 0x0046},    // F
    {0x0047, 0x0047},    // G
    {0x0048, 0x0048},    // H
    {0x0049, 0x0049},    // I
    {0x004a, 0x004a},    // J
    {0x004b, 0x004b},    // K
    {0x004c, 0x004c},    // L
    {0x004d, 0x004d},    // M
    {0x004e, 0x004e},    // N
    {0x004f, 0x004f},    // O
    {0x0050, 0x0050},    // P
    {0x0051, 0x0051},    // Q
    {0x0052, 0x0052},    // R
    {0x0053, 0x0053},    // S
    {0x0054, 0x0054},    // T
    {0x0055, 0x0055},    // U
    {0x0056, 0x0056},    // V
    {0x0057, 0x0057},    // W
    {0x0058, 0x0058},    // X
    {0x0059, 0x0059},    // Y
    {0x005a, 0x005a},    // Z
    {0x005b, 0x005b},    // bracketleft
    {0x005c, 0x005c},    // backslash
    {0x005d, 0x005d},    // bracketright
    {0x005e, 0x005e},    // asciicircum
    {0x005f, 0x005f},    // underscore
    {0x0060, 0x0060},    // grave
    {0x0060, 0x0060},    // quoteleft /* deprecated */
    {0x0061, 0x0061},    // a
    {0x0062, 0x0062},    // b
    {0x0063, 0x0063},    // c
    {0x0064, 0x0064},    // d
    {0x0065, 0x0065},    // e
    {0x0066, 0x0066},    // f
    {0x0067, 0x0067},    // g
    {0x0068, 0x0068},    // h
    {0x0069, 0x0069},    // i
    {0x006a, 0x006a},    // j
    {0x006b, 0x006b},    // k
    {0x006c, 0x006c},    // l
    {0x006d, 0x006d},    // m
    {0x006e, 0x006e},    // n
    {0x006f, 0x006f},    // o
    {0x0070, 0x0070},    // p
    {0x0071, 0x0071},    // q
    {0x0072, 0x0072},    // r
    {0x0073, 0x0073},    // s
    {0x0074, 0x0074},    // t
    {0x0075, 0x0075},    // u
    {0x0076, 0x0076},    // v
    {0x0077, 0x0077},    // w
    {0x0078, 0x0078},    // x
    {0x0079, 0x0079},    // y
    {0x007a, 0x007a},    // z
    {0x007b, 0x007b},    // braceleft
    {0x007c, 0x007c},    // bar
    {0x007d, 0x007d},    // braceright
    {0x007e, 0x007e},    // asciitilde
    {0x00a0, 0x00a0},    // nobreakspace
    {0x00a1, 0x00a1},    // exclamdown
    {0x00a2, 0x00a2},    // cent
    {0x00a3, 0x00a3},    // sterling
    {0x00a4, 0x00a4},    // currency
    {0x00a5, 0x00a5},    // yen
    {0x00a6, 0x00a6},    // brokenbar
    {0x00a7, 0x00a7},    // section
    {0x00a8, 0x00a8},    // diaeresis
    {0x00a9, 0x00a9},    // copyright
    {0x00aa, 0x00aa},    // ordfeminine
    {0x00ab, 0x00ab},    // guillemotleft /* left angle quotation mark */
    {0x00ac, 0x00ac},    // notsign
    {0x00ad, 0x00ad},    // hyphen
    {0x00ae, 0x00ae},    // registered
    {0x00af, 0x00af},    // macron
    {0x00b0, 0x00b0},    // degree
    {0x00b1, 0x00b1},    // plusminus
    {0x00b2, 0x00b2},    // twosuperior
    {0x00b3, 0x00b3},    // threesuperior
    {0x00b4, 0x00b4},    // acute
    {0x00b5, 0x00b5},    // mu
    {0x00b6, 0x00b6},    // paragraph
    {0x00b7, 0x00b7},    // periodcentered
    {0x00b8, 0x00b8},    // cedilla
    {0x00b9, 0x00b9},    // onesuperior
    {0x00ba, 0x00ba},    // masculine
    {0x00bb, 0x00bb},    // guillemotright /* right angle quotation mark */
    {0x00bc, 0x00bc},    // onequarter
    {0x00bd, 0x00bd},    // onehalf
    {0x00be, 0x00be},    // threequarters
    {0x00bf, 0x00bf},    // questiondown
    {0x00c0, 0x00c0},    // Agrave
    {0x00c1, 0x00c1},    // Aacute
    {0x00c2, 0x00c2},    // Acircumflex
    {0x00c3, 0x00c3},    // Atilde
    {0x00c4, 0x00c4},    // Adiaeresis
    {0x00c5, 0x00c5},    // Aring
    {0x00c6, 0x00c6},    // AE
    {0x00c7, 0x00c7},    // Ccedilla
    {0x00c8, 0x00c8},    // Egrave
    {0x00c9, 0x00c9},    // Eacute
    {0x00ca, 0x00ca},    // Ecircumflex
    {0x00cb, 0x00cb},    // Ediaeresis
    {0x00cc, 0x00cc},    // Igrave
    {0x00cd, 0x00cd},    // Iacute
    {0x00ce, 0x00ce},    // Icircumflex
    {0x00cf, 0x00cf},    // Idiaeresis
    {0x00d0, 0x00d0},    // ETH
    {0x00d0, 0x00d0},    // Eth /* deprecated */
    {0x00d1, 0x00d1},    // Ntilde
    {0x00d2, 0x00d2},    // Ograve
    {0x00d3, 0x00d3},    // Oacute
    {0x00d4, 0x00d4},    // Ocircumflex
    {0x00d5, 0x00d5},    // Otilde
    {0x00d6, 0x00d6},    // Odiaeresis
    {0x00d7, 0x00d7},    // multiply
    {0x00d8, 0x00d8},    // Ooblique
    {0x00d9, 0x00d9},    // Ugrave
    {0x00da, 0x00da},    // Uacute
    {0x00db, 0x00db},    // Ucircumflex
    {0x00dc, 0x00dc},    // Udiaeresis
    {0x00dd, 0x00dd},    // Yacute
    {0x00de, 0x00de},    // THORN
    {0x00de, 0x00de},    // Thorn /* deprecated */
    {0x00df, 0x00df},    // ssharp
    {0x00e0, 0x00e0},    // agrave
    {0x00e1, 0x00e1},    // aacute
    {0x00e2, 0x00e2},    // acircumflex
    {0x00e3, 0x00e3},    // atilde
    {0x00e4, 0x00e4},    // adiaeresis
    {0x00e5, 0x00e5},    // aring
    {0x00e6, 0x00e6},    // ae
    {0x00e7, 0x00e7},    // ccedilla
    {0x00e8, 0x00e8},    // egrave
    {0x00e9, 0x00e9},    // eacute
    {0x00ea, 0x00ea},    // ecircumflex
    {0x00eb, 0x00eb},    // ediaeresis
    {0x00ec, 0x00ec},    // igrave
    {0x00ed, 0x00ed},    // iacute
    {0x00ee, 0x00ee},    // icircumflex
    {0x00ef, 0x00ef},    // idiaeresis
    {0x00f0, 0x00f0},    // eth
    {0x00f1, 0x00f1},    // ntilde
    {0x00f2, 0x00f2},    // ograve
    {0x00f3, 0x00f3},    // oacute
    {0x00f4, 0x00f4},    // ocircumflex
    {0x00f5, 0x00f5},    // otilde
    {0x00f6, 0x00f6},    // odiaeresis
    {0x00f7, 0x00f7},    // division
    {0x00f8, 0x00f8},    // oslash
    {0x00f9, 0x00f9},    // ugrave
    {0x00fa, 0x00fa},    // uacute
    {0x00fb, 0x00fb},    // ucircumflex
    {0x00fc, 0x00fc},    // udiaeresis
    {0x00fd, 0x00fd},    // yacute
    {0x00fe, 0x00fe},    // thorn
    {0x00ff, 0x00ff},    // ydiaeresis
    {0x01a1, 0x0104},    // Aogonek
    {0x01a2, 0x02d8},    // breve
    {0x01a3, 0x0141},    // Lstroke
    {0x01a5, 0x013d},    // Lcaron
    {0x01a6, 0x015a},    // Sacute
    {0x01a9, 0x0160},    // Scaron
    {0x01aa, 0x015e},    // Scedilla
    {0x01ab, 0x0164},    // Tcaron
    {0x01ac, 0x0179},    // Zacute
    {0x01ae, 0x017d},    // Zcaron
    {0x01af, 0x017b},    // Zabovedot
    {0x01b1, 0x0105},    // aogonek
    {0x01b2, 0x02db},    // ogonek
    {0x01b3, 0x0142},    // lstroke
    {0x01b5, 0x013e},    // lcaron
    {0x01b6, 0x015b},    // sacute
    {0x01b7, 0x02c7},    // caron
    {0x01b9, 0x0161},    // scaron
    {0x01ba, 0x015f},    // scedilla
    {0x01bb, 0x0165},    // tcaron
    {0x01bc, 0x017a},    // zacute
    {0x01bd, 0x02dd},    // doubleacute
    {0x01be, 0x017e},    // zcaron
    {0x01bf, 0x017c},    // zabovedot
    {0x01c0, 0x0154},    // Racute
    {0x01c3, 0x0102},    // Abreve
    {0x01c5, 0x0139},    // Lacute
    {0x01c6, 0x0106},    // Cacute
    {0x01c8, 0x010c},    // Ccaron
    {0x01ca, 0x0118},    // Eogonek
    {0x01cc, 0x011a},    // Ecaron
    {0x01cf, 0x010e},    // Dcaron
    {0x01d0, 0x0110},    // Dstroke
    {0x01d1, 0x0143},    // Nacute
    {0x01d2, 0x0147},    // Ncaron
    {0x01d5, 0x0150},    // Odoubleacute
    {0x01d8, 0x0158},    // Rcaron
    {0x01d9, 0x016e},    // Uring
    {0x01db, 0x0170},    // Udoubleacute
    {0x01de, 0x0162},    // Tcedilla
    {0x01e0, 0x0155},    // racute
    {0x01e3, 0x0103},    // abreve
    {0x01e5, 0x013a},    // lacute
    {0x01e6, 0x0107},    // cacute
    {0x01e8, 0x010d},    // ccaron
    {0x01ea, 0x0119},    // eogonek
    {0x01ec, 0x011b},    // ecaron
    {0x01ef, 0x010f},    // dcaron
    {0x01f0, 0x0111},    // dstroke
    {0x01f1, 0x0144},    // nacute
    {0x01f2, 0x0148},    // ncaron
    {0x01f5, 0x0151},    // odoubleacute
    {0x01f8, 0x0159},    // rcaron
    {0x01f9, 0x016f},    // uring
    {0x01fb, 0x0171},    // udoubleacute
    {0x01fe, 0x0163},    // tcedilla
    {0x01ff, 0x02d9},    // abovedot
    {0x02a1, 0x0126},    // Hstroke
    {0x02a6, 0x0124},    // Hcircumflex
    {0x02a9, 0x0130},    // Iabovedot
    {0x02ab, 0x011e},    // Gbreve
    {0x02ac, 0x0134},    // Jcircumflex
    {0x02b1, 0x0127},    // hstroke
    {0x02b6, 0x0125},    // hcircumflex
    {0x02b9, 0x0131},    // idotless
    {0x02bb, 0x011f},    // gbreve
    {0x02bc, 0x0135},    // jcircumflex
    {0x02c5, 0x010a},    // Cabovedot
    {0x02c6, 0x0108},    // Ccircumflex
    {0x02d5, 0x0120},    // Gabovedot
    {0x02d8, 0x011c},    // Gcircumflex
    {0x02dd, 0x016c},    // Ubreve
    {0x02de, 0x015c},    // Scircumflex
    {0x02e5, 0x010b},    // cabovedot
    {0x02e6, 0x0109},    // ccircumflex
    {0x02f5, 0x0121},    // gabovedot
    {0x02f8, 0x011d},    // gcircumflex
    {0x02fd, 0x016d},    // ubreve
    {0x02fe, 0x015d},    // scircumflex
    {0x03a2, 0x0138},    // kra
    {0x03a3, 0x0156},    // Rcedilla
    {0x03a5, 0x0128},    // Itilde
    {0x03a6, 0x013b},    // Lcedilla
    {0x03aa, 0x0112},    // Emacron
    {0x03ab, 0x0122},    // Gcedilla
    {0x03ac, 0x0166},    // Tslash
    {0x03b3, 0x0157},    // rcedilla
    {0x03b5, 0x0129},    // itilde
    {0x03b6, 0x013c},    // lcedilla
    {0x03ba, 0x0113},    // emacron
    {0x03bb, 0x0123},    // gcedilla
    {0x03bc, 0x0167},    // tslash
    {0x03bd, 0x014a},    // ENG
    {0x03bf, 0x014b},    // eng
    {0x03c0, 0x0100},    // Amacron
    {0x03c7, 0x012e},    // Iogonek
    {0x03cc, 0x0116},    // Eabovedot
    {0x03cf, 0x012a},    // Imacron
    {0x03d1, 0x0145},    // Ncedilla
    {0x03d2, 0x014c},    // Omacron
    {0x03d3, 0x0136},    // Kcedilla
    {0x03d9, 0x0172},    // Uogonek
    {0x03dd, 0x0168},    // Utilde
    {0x03de, 0x016a},    // Umacron
    {0x03e0, 0x0101},    // amacron
    {0x03e7, 0x012f},    // iogonek
    {0x03ec, 0x0117},    // eabovedot
    {0x03ef, 0x012b},    // imacron
    {0x03f1, 0x0146},    // ncedilla
    {0x03f2, 0x014d},    // omacron
    {0x03f3, 0x0137},    // kcedilla
    {0x03f9, 0x0173},    // uogonek
    {0x03fd, 0x0169},    // utilde
    {0x03fe, 0x016b},    // umacron
    {0x047e, 0x203e},    // overline
    {0x04a1, 0x3002},    // kana_fullstop
    {0x04a2, 0x300c},    // kana_openingbracket
    {0x04a3, 0x300d},    // kana_closingbracket
    {0x04a4, 0x3001},    // kana_comma
    {0x04a5, 0x30fb},    // kana_conjunctive
    {0x04a6, 0x30f2},    // kana_WO
    {0x04a7, 0x30a1},    // kana_a
    {0x04a8, 0x30a3},    // kana_i
    {0x04a9, 0x30a5},    // kana_u
    {0x04aa, 0x30a7},    // kana_e
    {0x04ab, 0x30a9},    // kana_o
    {0x04ac, 0x30e3},    // kana_ya
    {0x04ad, 0x30e5},    // kana_yu
    {0x04ae, 0x30e7},    // kana_yo
    {0x04af, 0x30c3},    // kana_tsu
    {0x04b0, 0x30fc},    // prolongedsound
    {0x04b1, 0x30a2},    // kana_A
    {0x04b2, 0x30a4},    // kana_I
    {0x04b3, 0x30a6},    // kana_U
    {0x04b4, 0x30a8},    // kana_E
    {0x04b5, 0x30aa},    // kana_O
    {0x04b6, 0x30ab},    // kana_KA
    {0x04b7, 0x30ad},    // kana_KI
    {0x04b8, 0x30af},    // kana_KU
    {0x04b9, 0x30b1},    // kana_KE
    {0x04ba, 0x30b3},    // kana_KO
    {0x04bb, 0x30b5},    // kana_SA
    {0x04bc, 0x30b7},    // kana_SHI
    {0x04bd, 0x30b9},    // kana_SU
    {0x04be, 0x30bb},    // kana_SE
    {0x04bf, 0x30bd},    // kana_SO
    {0x04c0, 0x30bf},    // kana_TA
    {0x04c1, 0x30c1},    // kana_CHI
    {0x04c2, 0x30c4},    // kana_TSU
    {0x04c3, 0x30c6},    // kana_TE
    {0x04c4, 0x30c8},    // kana_TO
    {0x04c5, 0x30ca},    // kana_NA
    {0x04c6, 0x30cb},    // kana_NI
    {0x04c7, 0x30cc},    // kana_NU
    {0x04c8, 0x30cd},    // kana_NE
    {0x04c9, 0x30ce},    // kana_NO
    {0x04ca, 0x30cf},    // kana_HA
    {0x04cb, 0x30d2},    // kana_HI
    {0x04cc, 0x30d5},    // kana_FU
    {0x04cd, 0x30d8},    // kana_HE
    {0x04ce, 0x30db},    // kana_HO
    {0x04cf, 0x30de},    // kana_MA
    {0x04d0, 0x30df},    // kana_MI
    {0x04d1, 0x30e0},    // kana_MU
    {0x04d2, 0x30e1},    // kana_ME
    {0x04d3, 0x30e2},    // kana_MO
    {0x04d4, 0x30e4},    // kana_YA
    {0x04d5, 0x30e6},    // kana_YU
    {0x04d6, 0x30e8},    // kana_YO
    {0x04d7, 0x30e9},    // kana_RA
    {0x04d8, 0x30ea},    // kana_RI
    {0x04d9, 0x30eb},    // kana_RU
    {0x04da, 0x30ec},    // kana_RE
    {0x04db, 0x30ed},    // kana_RO
    {0x04dc, 0x30ef},    // kana_WA
    {0x04dd, 0x30f3},    // kana_N
    {0x04de, 0x309b},    // voicedsound
    {0x04df, 0x309c},    // semivoicedsound
    {0x05ac, 0x060c},    // Arabic_comma
    {0x05bb, 0x061b},    // Arabic_semicolon
    {0x05bf, 0x061f},    // Arabic_question_mark
    {0x05c1, 0x0621},    // Arabic_hamza
    {0x05c2, 0x0622},    // Arabic_maddaonalef
    {0x05c3, 0x0623},    // Arabic_hamzaonalef
    {0x05c4, 0x0624},    // Arabic_hamzaonwaw
    {0x05c5, 0x0625},    // Arabic_hamzaunderalef
    {0x05c6, 0x0626},    // Arabic_hamzaonyeh
    {0x05c7, 0x0627},    // Arabic_alef
    {0x05c8, 0x0628},    // Arabic_beh
    {0x05c9, 0x0629},    // Arabic_tehmarbuta
    {0x05ca, 0x062a},    // Arabic_teh
    {0x05cb, 0x062b},    // Arabic_theh
    {0x05cc, 0x062c},    // Arabic_jeem
    {0x05cd, 0x062d},    // Arabic_hah
    {0x05ce, 0x062e},    // Arabic_khah
    {0x05cf, 0x062f},    // Arabic_dal
    {0x05d0, 0x0630},    // Arabic_thal
    {0x05d1, 0x0631},    // Arabic_ra
    {0x05d2, 0x0632},    // Arabic_zain
    {0x05d3, 0x0633},    // Arabic_seen
    {0x05d4, 0x0634},    // Arabic_sheen
    {0x05d5, 0x0635},    // Arabic_sad
    {0x05d6, 0x0636},    // Arabic_dad
    {0x05d7, 0x0637},    // Arabic_tah
    {0x05d8, 0x0638},    // Arabic_zah
    {0x05d9, 0x0639},    // Arabic_ain
    {0x05da, 0x063a},    // Arabic_ghain
    {0x05e0, 0x0640},    // Arabic_tatweel
    {0x05e1, 0x0641},    // Arabic_feh
    {0x05e2, 0x0642},    // Arabic_qaf
    {0x05e3, 0x0643},    // Arabic_kaf
    {0x05e4, 0x0644},    // Arabic_lam
    {0x05e5, 0x0645},    // Arabic_meem
    {0x05e6, 0x0646},    // Arabic_noon
    {0x05e7, 0x0647},    // Arabic_ha
    {0x05e8, 0x0648},    // Arabic_waw
    {0x05e9, 0x0649},    // Arabic_alefmaksura
    {0x05ea, 0x064a},    // Arabic_yeh
    {0x05eb, 0x064b},    // Arabic_fathatan
    {0x05ec, 0x064c},    // Arabic_dammatan
    {0x05ed, 0x064d},    // Arabic_kasratan
    {0x05ee, 0x064e},    // Arabic_fatha
    {0x05ef, 0x064f},    // Arabic_damma
    {0x05f0, 0x0650},    // Arabic_kasra
    {0x05f1, 0x0651},    // Arabic_shadda
    {0x05f2, 0x0652},    // Arabic_sukun
    {0x06a1, 0x0452},    // Serbian_dje
    {0x06a2, 0x0453},    // Macedonia_gje
    {0x06a3, 0x0451},    // Cyrillic_io
    {0x06a4, 0x0454},    // Ukrainian_ie
    {0x06a5, 0x0455},    // Macedonia_dse
    {0x06a6, 0x0456},    // Ukrainian_i
    {0x06a7, 0x0457},    // Ukrainian_yi
    {0x06a8, 0x0458},    // Cyrillic_je
    {0x06a9, 0x0459},    // Cyrillic_lje
    {0x06aa, 0x045a},    // Cyrillic_nje
    {0x06ab, 0x045b},    // Serbian_tshe
    {0x06ac, 0x045c},    // Macedonia_kje
    {0x06ae, 0x045e},    // Byelorussian_shortu
    {0x06af, 0x045f},    // Cyrillic_dzhe
    {0x06b0, 0x2116},    // numerosign
    {0x06b1, 0x0402},    // Serbian_DJE
    {0x06b2, 0x0403},    // Macedonia_GJE
    {0x06b3, 0x0401},    // Cyrillic_IO
    {0x06b4, 0x0404},    // Ukrainian_IE
    {0x06b5, 0x0405},    // Macedonia_DSE
    {0x06b6, 0x0406},    // Ukrainian_I
    {0x06b7, 0x0407},    // Ukrainian_YI
    {0x06b8, 0x0408},    // Cyrillic_JE
    {0x06b9, 0x0409},    // Cyrillic_LJE
    {0x06ba, 0x040a},    // Cyrillic_NJE
    {0x06bb, 0x040b},    // Serbian_TSHE
    {0x06bc, 0x040c},    // Macedonia_KJE
    {0x06be, 0x040e},    // Byelorussian_SHORTU
    {0x06bf, 0x040f},    // Cyrillic_DZHE
    {0x06c0, 0x044e},    // Cyrillic_yu
    {0x06c1, 0x0430},    // Cyrillic_a
    {0x06c2, 0x0431},    // Cyrillic_be
    {0x06c3, 0x0446},    // Cyrillic_tse
    {0x06c4, 0x0434},    // Cyrillic_de
    {0x06c5, 0x0435},    // Cyrillic_ie
    {0x06c6, 0x0444},    // Cyrillic_ef
    {0x06c7, 0x0433},    // Cyrillic_ghe
    {0x06c8, 0x0445},    // Cyrillic_ha
    {0x06c9, 0x0438},    // Cyrillic_i
    {0x06ca, 0x0439},    // Cyrillic_shorti
    {0x06cb, 0x043a},    // Cyrillic_ka
    {0x06cc, 0x043b},    // Cyrillic_el
    {0x06cd, 0x043c},    // Cyrillic_em
    {0x06ce, 0x043d},    // Cyrillic_en
    {0x06cf, 0x043e},    // Cyrillic_o
    {0x06d0, 0x043f},    // Cyrillic_pe
    {0x06d1, 0x044f},    // Cyrillic_ya
    {0x06d2, 0x0440},    // Cyrillic_er
    {0x06d3, 0x0441},    // Cyrillic_es
    {0x06d4, 0x0442},    // Cyrillic_te
    {0x06d5, 0x0443},    // Cyrillic_u
    {0x06d6, 0x0436},    // Cyrillic_zhe
    {0x06d7, 0x0432},    // Cyrillic_ve
    {0x06d8, 0x044c},    // Cyrillic_softsign
    {0x06d9, 0x044b},    // Cyrillic_yeru
    {0x06da, 0x0437},    // Cyrillic_ze
    {0x06db, 0x0448},    // Cyrillic_sha
    {0x06dc, 0x044d},    // Cyrillic_e
    {0x06dd, 0x0449},    // Cyrillic_shcha
    {0x06de, 0x0447},    // Cyrillic_che
    {0x06df, 0x044a},    // Cyrillic_hardsign
    {0x06e0, 0x042e},    // Cyrillic_YU
    {0x06e1, 0x0410},    // Cyrillic_A
    {0x06e2, 0x0411},    // Cyrillic_BE
    {0x06e3, 0x0426},    // Cyrillic_TSE
    {0x06e4, 0x0414},    // Cyrillic_DE
    {0x06e5, 0x0415},    // Cyrillic_IE
    {0x06e6, 0x0424},    // Cyrillic_EF
    {0x06e7, 0x0413},    // Cyrillic_GHE
    {0x06e8, 0x0425},    // Cyrillic_HA
    {0x06e9, 0x0418},    // Cyrillic_I
    {0x06ea, 0x0419},    // Cyrillic_SHORTI
    {0x06eb, 0x041a},    // Cyrillic_KA
    {0x06ec, 0x041b},    // Cyrillic_EL
    {0x06ed, 0x041c},    // Cyrillic_EM
    {0x06ee, 0x041d},    // Cyrillic_EN
    {0x06ef, 0x041e},    // Cyrillic_O
    {0x06f0, 0x041f},    // Cyrillic_PE
    {0x06f1, 0x042f},    // Cyrillic_YA
    {0x06f2, 0x0420},    // Cyrillic_ER
    {0x06f3, 0x0421},    // Cyrillic_ES
    {0x06f4, 0x0422},    // Cyrillic_TE
    {0x06f5, 0x0423},    // Cyrillic_U
    {0x06f6, 0x0416},    // Cyrillic_ZHE
    {0x06f7, 0x0412},    // Cyrillic_VE
    {0x06f8, 0x042c},    // Cyrillic_SOFTSIGN
    {0x06f9, 0x042b},    // Cyrillic_YERU
    {0x06fa, 0x0417},    // Cyrillic_ZE
    {0x06fb, 0x0428},    // Cyrillic_SHA
    {0x06fc, 0x042d},    // Cyrillic_E
    {0x06fd, 0x0429},    // Cyrillic_SHCHA
    {0x06fe, 0x0427},    // Cyrillic_CHE
    {0x06ff, 0x042a},    // Cyrillic_HARDSIGN
    {0x07a1, 0x0386},    // Greek_ALPHAaccent
    {0x07a2, 0x0388},    // Greek_EPSILONaccent
    {0x07a3, 0x0389},    // Greek_ETAaccent
    {0x07a4, 0x038a},    // Greek_IOTAaccent
    {0x07a5, 0x03aa},    // Greek_IOTAdiaeresis
    {0x07a7, 0x038c},    // Greek_OMICRONaccent
    {0x07a8, 0x038e},    // Greek_UPSILONaccent
    {0x07a9, 0x03ab},    // Greek_UPSILONdieresis
    {0x07ab, 0x038f},    // Greek_OMEGAaccent
    {0x07ae, 0x0385},    // Greek_accentdieresis
    {0x07af, 0x2015},    // Greek_horizbar
    {0x07b1, 0x03ac},    // Greek_alphaaccent
    {0x07b2, 0x03ad},    // Greek_epsilonaccent
    {0x07b3, 0x03ae},    // Greek_etaaccent
    {0x07b4, 0x03af},    // Greek_iotaaccent
    {0x07b5, 0x03ca},    // Greek_iotadieresis
    {0x07b6, 0x0390},    // Greek_iotaaccentdieresis
    {0x07b7, 0x03cc},    // Greek_omicronaccent
    {0x07b8, 0x03cd},    // Greek_upsilonaccent
    {0x07b9, 0x03cb},    // Greek_upsilondieresis
    {0x07ba, 0x03b0},    // Greek_upsilonaccentdieresis
    {0x07bb, 0x03ce},    // Greek_omegaaccent
    {0x07c1, 0x0391},    // Greek_ALPHA
    {0x07c2, 0x0392},    // Greek_BETA
    {0x07c3, 0x0393},    // Greek_GAMMA
    {0x07c4, 0x0394},    // Greek_DELTA
    {0x07c5, 0x0395},    // Greek_EPSILON
    {0x07c6, 0x0396},    // Greek_ZETA
    {0x07c7, 0x0397},    // Greek_ETA
    {0x07c8, 0x0398},    // Greek_THETA
    {0x07c9, 0x0399},    // Greek_IOTA
    {0x07ca, 0x039a},    // Greek_KAPPA
    {0x07cb, 0x039b},    // Greek_LAMBDA
    {0x07cb, 0x039b},    // Greek_LAMDA
    {0x07cc, 0x039c},    // Greek_MU
    {0x07cd, 0x039d},    // Greek_NU
    {0x07ce, 0x039e},    // Greek_XI
    {0x07cf, 0x039f},    // Greek_OMICRON
    {0x07d0, 0x03a0},    // Greek_PI
    {0x07d1, 0x03a1},    // Greek_RHO
    {0x07d2, 0x03a3},    // Greek_SIGMA
    {0x07d4, 0x03a4},    // Greek_TAU
    {0x07d5, 0x03a5},    // Greek_UPSILON
    {0x07d6, 0x03a6},    // Greek_PHI
    {0x07d7, 0x03a7},    // Greek_CHI
    {0x07d8, 0x03a8},    // Greek_PSI
    {0x07d9, 0x03a9},    // Greek_OMEGA
    {0x07e1, 0x03b1},    // Greek_alpha
    {0x07e2, 0x03b2},    // Greek_beta
    {0x07e3, 0x03b3},    // Greek_gamma
    {0x07e4, 0x03b4},    // Greek_delta
    {0x07e5, 0x03b5},    // Greek_epsilon
    {0x07e6, 0x03b6},    // Greek_zeta
    {0x07e7, 0x03b7},    // Greek_eta
    {0x07e8, 0x03b8},    // Greek_theta
    {0x07e9, 0x03b9},    // Greek_iota
    {0x07ea, 0x03ba},    // Greek_kappa
    {0x07eb, 0x03bb},    // Greek_lambda
    {0x07ec, 0x03bc},    // Greek_mu
    {0x07ed, 0x03bd},    // Greek_nu
    {0x07ee, 0x03be},    // Greek_xi
    {0x07ef, 0x03bf},    // Greek_omicron
    {0x07f0, 0x03c0},    // Greek_pi
    {0x07f1, 0x03c1},    // Greek_rho
    {0x07f2, 0x03c3},    // Greek_sigma
    {0x07f3, 0x03c2},    // Greek_finalsmallsigma
    {0x07f4, 0x03c4},    // Greek_tau
    {0x07f5, 0x03c5},    // Greek_upsilon
    {0x07f6, 0x03c6},    // Greek_phi
    {0x07f7, 0x03c7},    // Greek_chi
    {0x07f8, 0x03c8},    // Greek_psi
    {0x07f9, 0x03c9},    // Greek_omega
    {0x08a1, 0x23b7},    // leftradical
    {0x08a2, 0x250c},    // topleftradical
    {0x08a3, 0x2500},    // horizconnector
    {0x08a4, 0x2320},    // topintegral
    {0x08a5, 0x2321},    // botintegral
    {0x08a6, 0x2502},    // vertconnector
    {0x08a7, 0x23a1},    // topleftsqbracket
    {0x08a8, 0x23a3},    // botleftsqbracket
    {0x08a9, 0x23a4},    // toprightsqbracket
    {0x08aa, 0x23a6},    // botrightsqbracket
    {0x08ab, 0x239b},    // topleftparens
    {0x08ac, 0x239d},    // botleftparens
    {0x08ad, 0x239e},    // toprightparens
    {0x08ae, 0x23a0},    // botrightparens
    {0x08af, 0x23a8},    // leftmiddlecurlybrace
    {0x08b0, 0x23ac},    // rightmiddlecurlybrace
    {0x08b1, 0x0000},    // topleftsummation
    {0x08b2, 0x0000},    // botleftsummation
    {0x08b3, 0x0000},    // topvertsummationconnector
    {0x08b4, 0x0000},    // botvertsummationconnector
    {0x08b5, 0x0000},    // toprightsummation
    {0x08b6, 0x0000},    // botrightsummation
    {0x08b7, 0x0000},    // rightmiddlesummation
    {0x08bc, 0x2264},    // lessthanequal
    {0x08bd, 0x2260},    // notequal
    {0x08be, 0x2265},    // greaterthanequal
    {0x08bf, 0x222b},    // integral
    {0x08c0, 0x2234},    // therefore
    {0x08c1, 0x221d},    // variation
    {0x08c2, 0x221e},    // infinity
    {0x08c5, 0x2207},    // nabla
    {0x08c8, 0x223c},    // approximate
    {0x08c9, 0x2243},    // similarequal
    {0x08cd, 0x21d4},    // ifonlyif
    {0x08ce, 0x21d2},    // implies
    {0x08cf, 0x2261},    // identical
    {0x08d6, 0x221a},    // radical
    {0x08da, 0x2282},    // includedin
    {0x08db, 0x2283},    // includes
    {0x08dc, 0x2229},    // intersection
    {0x08dd, 0x222a},    // union
    {0x08de, 0x2227},    // logicaland
    {0x08df, 0x2228},    // logicalor
    {0x08ef, 0x2202},    // partialderivative
    {0x08f6, 0x0192},    // function
    {0x08fb, 0x2190},    // leftarrow
    {0x08fc, 0x2191},    // uparrow
    {0x08fd, 0x2192},    // rightarrow
    {0x08fe, 0x2193},    // downarrow
    {0x09df, 0x0000},    // blank
    {0x09e0, 0x25c6},    // soliddiamond
    {0x09e1, 0x2592},    // checkerboard
    {0x09e2, 0x2409},    // ht
    {0x09e3, 0x240c},    // ff
    {0x09e4, 0x240d},    // cr
    {0x09e5, 0x240a},    // lf
    {0x09e8, 0x2424},    // nl
    {0x09e9, 0x240b},    // vt
    {0x09ea, 0x2518},    // lowrightcorner
    {0x09eb, 0x2510},    // uprightcorner
    {0x09ec, 0x250c},    // upleftcorner
    {0x09ed, 0x2514},    // lowleftcorner
    {0x09ee, 0x253c},    // crossinglines
    {0x09ef, 0x23ba},    // horizlinescan1
    {0x09f0, 0x23bb},    // horizlinescan3
    {0x09f1, 0x2500},    // horizlinescan5
    {0x09f2, 0x23bc},    // horizlinescan7
    {0x09f3, 0x23bd},    // horizlinescan9
    {0x09f4, 0x251c},    // leftt
    {0x09f5, 0x2524},    // rightt
    {0x09f6, 0x2534},    // bott
    {0x09f7, 0x252c},    // topt
    {0x09f8, 0x2502},    // vertbar
    {0x0aa1, 0x2003},    // emspace
    {0x0aa2, 0x2002},    // enspace
    {0x0aa3, 0x2004},    // em3space
    {0x0aa4, 0x2005},    // em4space
    {0x0aa5, 0x2007},    // digitspace
    {0x0aa6, 0x2008},    // punctspace
    {0x0aa7, 0x2009},    // thinspace
    {0x0aa8, 0x200a},    // hairspace
    {0x0aa9, 0x2014},    // emdash
    {0x0aaa, 0x2013},    // endash
    {0x0aac, 0x2423},    // signifblank
    {0x0aae, 0x2026},    // ellipsis
    {0x0aaf, 0x2025},    // doubbaselinedot
    {0x0ab0, 0x2153},    // onethird
    {0x0ab1, 0x2154},    // twothirds
    {0x0ab2, 0x2155},    // onefifth
    {0x0ab3, 0x2156},    // twofifths
    {0x0ab4, 0x2157},    // threefifths
    {0x0ab5, 0x2158},    // fourfifths
    {0x0ab6, 0x2159},    // onesixth
    {0x0ab7, 0x215a},    // fivesixths
    {0x0ab8, 0x2105},    // careof
    {0x0abb, 0x2012},    // figdash
    {0x0abc, 0x27e8},    // leftanglebracket
    {0x0abd, 0x002e},    // decimalpoint
    {0x0abe, 0x27e9},    // rightanglebracket
    {0x0abf, 0x0000},    // marker
    {0x0ac3, 0x215b},    // oneeighth
    {0x0ac4, 0x215c},    // threeeighths
    {0x0ac5, 0x215d},    // fiveeighths
    {0x0ac6, 0x215e},    // seveneighths
    {0x0ac9, 0x2122},    // trademark
    {0x0aca, 0x2613},    // signaturemark
    {0x0acb, 0x0000},    // trademarkincircle
    {0x0acc, 0x25c1},    // leftopentriangle
    {0x0acd, 0x25b7},    // rightopentriangle
    {0x0ace, 0x25cb},    // emopencircle
    {0x0acf, 0x25af},    // emopenrectangle
    {0x0ad0, 0x2018},    // leftsinglequotemark
    {0x0ad1, 0x2019},    // rightsinglequotemark
    {0x0ad2, 0x201c},    // leftdoublequotemark
    {0x0ad3, 0x201d},    // rightdoublequotemark
    {0x0ad4, 0x211e},    // prescription
    {0x0ad6, 0x2032},    // minutes
    {0x0ad7, 0x2033},    // seconds
    {0x0ad9, 0x271d},    // latincross
    {0x0ada, 0x0000},    // hexagram
    {0x0adb, 0x25ac},    // filledrectbullet
    {0x0adc, 0x25c0},    // filledlefttribullet
    {0x0add, 0x25b6},    // filledrighttribullet
    {0x0ade, 0x25cf},    // emfilledcircle
    {0x0adf, 0x25ae},    // emfilledrect
    {0x0ae0, 0x25e6},    // enopencircbullet
    {0x0ae1, 0x25ab},    // enopensquarebullet
    {0x0ae2, 0x25ad},    // openrectbullet
    {0x0ae3, 0x25b3},    // opentribulletup
    {0x0ae4, 0x25bd},    // opentribulletdown
    {0x0ae5, 0x2606},    // openstar
    {0x0ae6, 0x2022},    // enfilledcircbullet
    {0x0ae7, 0x25aa},    // enfilledsqbullet
    {0x0ae8, 0x25b2},    // filledtribulletup
    {0x0ae9, 0x25bc},    // filledtribulletdown
    {0x0aea, 0x261c},    // leftpointer
    {0x0aeb, 0x261e},    // rightpointer
    {0x0aec, 0x2663},    // club
    {0x0aed, 0x2666},    // diamond
    {0x0aee, 0x2665},    // heart
    {0x0af0, 0x2720},    // maltesecross
    {0x0af1, 0x2020},    // dagger
    {0x0af2, 0x2021},    // doubledagger
    {0x0af3, 0x2713},    // checkmark
    {0x0af4, 0x2717},    // ballotcross
    {0x0af5, 0x266f},    // musicalsharp
    {0x0af6, 0x266d},    // musicalflat
    {0x0af7, 0x2642},    // malesymbol
    {0x0af8, 0x2640},    // femalesymbol
    {0x0af9, 0x260e},    // telephone
    {0x0afa, 0x2315},    // telephonerecorder
    {0x0afb, 0x2117},    // phonographcopyright
    {0x0afc, 0x2038},    // caret
    {0x0afd, 0x201a},    // singlelowquotemark
    {0x0afe, 0x201e},    // doublelowquotemark
    {0x0aff, 0x0000},    // cursor
    {0x0ba3, 0x003c},    // leftcaret
    {0x0ba6, 0x003e},    // rightcaret
    {0x0ba8, 0x2228},    // downcaret
    {0x0ba9, 0x2227},    // upcaret
    {0x0bc0, 0x00af},    // overbar
    {0x0bc2, 0x22a5},    // downtack
    {0x0bc3, 0x2229},    // upshoe
    {0x0bc4, 0x230a},    // downstile
    {0x0bc6, 0x005f},    // underbar
    {0x0bca, 0x2218},    // jot
    {0x0bcc, 0x2395},    // quad
    {0x0bce, 0x22a4},    // uptack
    {0x0bcf, 0x25cb},    // circle
    {0x0bd3, 0x2308},    // upstile
    {0x0bd6, 0x222a},    // downshoe
    {0x0bd8, 0x2283},    // rightshoe
    {0x0bda, 0x2282},    // leftshoe
    {0x0bdc, 0x22a2},    // lefttack
    {0x0bfc, 0x22a3},    // righttack
    {0x0cdf, 0x2017},    // hebrew_doublelowline
    {0x0ce0, 0x05d0},    // hebrew_aleph
    {0x0ce1, 0x05d1},    // hebrew_bet
    {0x0ce1, 0x05d1},    // hebrew_beth  /* deprecated */
    {0x0ce2, 0x05d2},    // hebrew_gimel
    {0x0ce2, 0x05d2},    // hebrew_gimmel  /* deprecated */
    {0x0ce3, 0x05d3},    // hebrew_dalet
    {0x0ce3, 0x05d3},    // hebrew_daleth  /* deprecated */
    {0x0ce4, 0x05d4},    // hebrew_he
    {0x0ce5, 0x05d5},    // hebrew_waw
    {0x0ce6, 0x05d6},    // hebrew_zain
    {0x0ce6, 0x05d6},    // hebrew_zayin  /* deprecated */
    {0x0ce7, 0x05d7},    // hebrew_chet
    {0x0ce7, 0x05d7},    // hebrew_het  /* deprecated */
    {0x0ce8, 0x05d8},    // hebrew_tet
    {0x0ce8, 0x05d8},    // hebrew_teth  /* deprecated */
    {0x0ce9, 0x05d9},    // hebrew_yod
    {0x0cea, 0x05da},    // hebrew_finalkaph
    {0x0ceb, 0x05db},    // hebrew_kaph
    {0x0cec, 0x05dc},    // hebrew_lamed
    {0x0ced, 0x05dd},    // hebrew_finalmem
    {0x0cee, 0x05de},    // hebrew_mem
    {0x0cef, 0x05df},    // hebrew_finalnun
    {0x0cf0, 0x05e0},    // hebrew_nun
    {0x0cf1, 0x05e1},    // hebrew_samech
    {0x0cf1, 0x05e1},    // hebrew_samekh  /* deprecated */
    {0x0cf2, 0x05e2},    // hebrew_ayin
    {0x0cf3, 0x05e3},    // hebrew_finalpe
    {0x0cf4, 0x05e4},    // hebrew_pe
    {0x0cf5, 0x05e5},    // hebrew_finalzade
    {0x0cf5, 0x05e5},    // hebrew_finalzadi  /* deprecated */
    {0x0cf6, 0x05e6},    // hebrew_zade
    {0x0cf6, 0x05e6},    // hebrew_zadi  /* deprecated */
    {0x0cf7, 0x05e7},    // hebrew_kuf  /* deprecated */
    {0x0cf7, 0x05e7},    // hebrew_qoph
    {0x0cf8, 0x05e8},    // hebrew_resh
    {0x0cf9, 0x05e9},    // hebrew_shin
    {0x0cfa, 0x05ea},    // hebrew_taf  /* deprecated */
    {0x0cfa, 0x05ea},    // hebrew_taw
    {0x0da1, 0x0e01},    // Thai_kokai
    {0x0da2, 0x0e02},    // Thai_khokhai
    {0x0da3, 0x0e03},    // Thai_khokhuat
    {0x0da4, 0x0e04},    // Thai_khokhwai
    {0x0da5, 0x0e05},    // Thai_khokhon
    {0x0da6, 0x0e06},    // Thai_khorakhang
    {0x0da7, 0x0e07},    // Thai_ngongu
    {0x0da8, 0x0e08},    // Thai_chochan
    {0x0da9, 0x0e09},    // Thai_choching
    {0x0daa, 0x0e0a},    // Thai_chochang
    {0x0dab, 0x0e0b},    // Thai_soso
    {0x0dac, 0x0e0c},    // Thai_chochoe
    {0x0dad, 0x0e0d},    // Thai_yoying
    {0x0dae, 0x0e0e},    // Thai_dochada
    {0x0daf, 0x0e0f},    // Thai_topatak
    {0x0db0, 0x0e10},    // Thai_thothan
    {0x0db1, 0x0e11},    // Thai_thonangmontho
    {0x0db2, 0x0e12},    // Thai_thophuthao
    {0x0db3, 0x0e13},    // Thai_nonen
    {0x0db4, 0x0e14},    // Thai_dodek
    {0x0db5, 0x0e15},    // Thai_totao
    {0x0db6, 0x0e16},    // Thai_thothung
    {0x0db7, 0x0e17},    // Thai_thothahan
    {0x0db8, 0x0e18},    // Thai_thothong
    {0x0db9, 0x0e19},    // Thai_nonu
    {0x0dba, 0x0e1a},    // Thai_bobaimai
    {0x0dbb, 0x0e1b},    // Thai_popla
    {0x0dbc, 0x0e1c},    // Thai_phophung
    {0x0dbd, 0x0e1d},    // Thai_fofa
    {0x0dbe, 0x0e1e},    // Thai_phophan
    {0x0dbf, 0x0e1f},    // Thai_fofan
    {0x0dc0, 0x0e20},    // Thai_phosamphao
    {0x0dc1, 0x0e21},    // Thai_moma
    {0x0dc2, 0x0e22},    // Thai_yoyak
    {0x0dc3, 0x0e23},    // Thai_rorua
    {0x0dc4, 0x0e24},    // Thai_ru
    {0x0dc5, 0x0e25},    // Thai_loling
    {0x0dc6, 0x0e26},    // Thai_lu
    {0x0dc7, 0x0e27},    // Thai_wowaen
    {0x0dc8, 0x0e28},    // Thai_sosala
    {0x0dc9, 0x0e29},    // Thai_sorusi
    {0x0dca, 0x0e2a},    // Thai_sosua
    {0x0dcb, 0x0e2b},    // Thai_hohip
    {0x0dcc, 0x0e2c},    // Thai_lochula
    {0x0dcd, 0x0e2d},    // Thai_oang
    {0x0dce, 0x0e2e},    // Thai_honokhuk
    {0x0dcf, 0x0e2f},    // Thai_paiyannoi
    {0x0dd0, 0x0e30},    // Thai_saraa
    {0x0dd1, 0x0e31},    // Thai_maihanakat
    {0x0dd2, 0x0e32},    // Thai_saraaa
    {0x0dd3, 0x0e33},    // Thai_saraam
    {0x0dd4, 0x0e34},    // Thai_sarai
    {0x0dd5, 0x0e35},    // Thai_saraii
    {0x0dd6, 0x0e36},    // Thai_saraue
    {0x0dd7, 0x0e37},    // Thai_sarauee
    {0x0dd8, 0x0e38},    // Thai_sarau
    {0x0dd9, 0x0e39},    // Thai_sarauu
    {0x0dda, 0x0e3a},    // Thai_phinthu
    {0x0dde, 0x0000},    // Thai_maihanakat_maitho
    {0x0ddf, 0x0e3f},    // Thai_baht
    {0x0de0, 0x0e40},    // Thai_sarae
    {0x0de1, 0x0e41},    // Thai_saraae
    {0x0de2, 0x0e42},    // Thai_sarao
    {0x0de3, 0x0e43},    // Thai_saraaimaimuan
    {0x0de4, 0x0e44},    // Thai_saraaimaimalai
    {0x0de5, 0x0e45},    // Thai_lakkhangyao
    {0x0de6, 0x0e46},    // Thai_maiyamok
    {0x0de7, 0x0e47},    // Thai_maitaikhu
    {0x0de8, 0x0e48},    // Thai_maiek
    {0x0de9, 0x0e49},    // Thai_maitho
    {0x0dea, 0x0e4a},    // Thai_maitri
    {0x0deb, 0x0e4b},    // Thai_maichattawa
    {0x0dec, 0x0e4c},    // Thai_thanthakhat
    {0x0ded, 0x0e4d},    // Thai_nikhahit
    {0x0df0, 0x0e50},    // Thai_leksun
    {0x0df1, 0x0e51},    // Thai_leknung
    {0x0df2, 0x0e52},    // Thai_leksong
    {0x0df3, 0x0e53},    // Thai_leksam
    {0x0df4, 0x0e54},    // Thai_leksi
    {0x0df5, 0x0e55},    // Thai_lekha
    {0x0df6, 0x0e56},    // Thai_lekhok
    {0x0df7, 0x0e57},    // Thai_lekchet
    {0x0df8, 0x0e58},    // Thai_lekpaet
    {0x0df9, 0x0e59},    // Thai_lekkao
    {0x0ea1, 0x3131},    // Hangul_Kiyeog
    {0x0ea2, 0x3132},    // Hangul_SsangKiyeog
    {0x0ea3, 0x3133},    // Hangul_KiyeogSios
    {0x0ea4, 0x3134},    // Hangul_Nieun
    {0x0ea5, 0x3135},    // Hangul_NieunJieuj
    {0x0ea6, 0x3136},    // Hangul_NieunHieuh
    {0x0ea7, 0x3137},    // Hangul_Dikeud
    {0x0ea8, 0x3138},    // Hangul_SsangDikeud
    {0x0ea9, 0x3139},    // Hangul_Rieul
    {0x0eaa, 0x313a},    // Hangul_RieulKiyeog
    {0x0eab, 0x313b},    // Hangul_RieulMieum
    {0x0eac, 0x313c},    // Hangul_RieulPieub
    {0x0ead, 0x313d},    // Hangul_RieulSios
    {0x0eae, 0x313e},    // Hangul_RieulTieut
    {0x0eaf, 0x313f},    // Hangul_RieulPhieuf
    {0x0eb0, 0x3140},    // Hangul_RieulHieuh
    {0x0eb1, 0x3141},    // Hangul_Mieum
    {0x0eb2, 0x3142},    // Hangul_Pieub
    {0x0eb3, 0x3143},    // Hangul_SsangPieub
    {0x0eb4, 0x3144},    // Hangul_PieubSios
    {0x0eb5, 0x3145},    // Hangul_Sios
    {0x0eb6, 0x3146},    // Hangul_SsangSios
    {0x0eb7, 0x3147},    // Hangul_Ieung
    {0x0eb8, 0x3148},    // Hangul_Jieuj
    {0x0eb9, 0x3149},    // Hangul_SsangJieuj
    {0x0eba, 0x314a},    // Hangul_Cieuc
    {0x0ebb, 0x314b},    // Hangul_Khieuq
    {0x0ebc, 0x314c},    // Hangul_Tieut
    {0x0ebd, 0x314d},    // Hangul_Phieuf
    {0x0ebe, 0x314e},    // Hangul_Hieuh
    {0x0ebf, 0x314f},    // Hangul_A
    {0x0ec0, 0x3150},    // Hangul_AE
    {0x0ec1, 0x3151},    // Hangul_YA
    {0x0ec2, 0x3152},    // Hangul_YAE
    {0x0ec3, 0x3153},    // Hangul_EO
    {0x0ec4, 0x3154},    // Hangul_E
    {0x0ec5, 0x3155},    // Hangul_YEO
    {0x0ec6, 0x3156},    // Hangul_YE
    {0x0ec7, 0x3157},    // Hangul_O
    {0x0ec8, 0x3158},    // Hangul_WA
    {0x0ec9, 0x3159},    // Hangul_WAE
    {0x0eca, 0x315a},    // Hangul_OE
    {0x0ecb, 0x315b},    // Hangul_YO
    {0x0ecc, 0x315c},    // Hangul_U
    {0x0ecd, 0x315d},    // Hangul_WEO
    {0x0ece, 0x315e},    // Hangul_WE
    {0x0ecf, 0x315f},    // Hangul_WI
    {0x0ed0, 0x3160},    // Hangul_YU
    {0x0ed1, 0x3161},    // Hangul_EU
    {0x0ed2, 0x3162},    // Hangul_YI
    {0x0ed3, 0x3163},    // Hangul_I
    {0x0ed4, 0x11a8},    // Hangul_J_Kiyeog
    {0x0ed5, 0x11a9},    // Hangul_J_SsangKiyeog
    {0x0ed6, 0x11aa},    // Hangul_J_KiyeogSios
    {0x0ed7, 0x11ab},    // Hangul_J_Nieun
    {0x0ed8, 0x11ac},    // Hangul_J_NieunJieuj
    {0x0ed9, 0x11ad},    // Hangul_J_NieunHieuh
    {0x0eda, 0x11ae},    // Hangul_J_Dikeud
    {0x0edb, 0x11af},    // Hangul_J_Rieul
    {0x0edc, 0x11b0},    // Hangul_J_RieulKiyeog
    {0x0edd, 0x11b1},    // Hangul_J_RieulMieum
    {0x0ede, 0x11b2},    // Hangul_J_RieulPieub
    {0x0edf, 0x11b3},    // Hangul_J_RieulSios
    {0x0ee0, 0x11b4},    // Hangul_J_RieulTieut
    {0x0ee1, 0x11b5},    // Hangul_J_RieulPhieuf
    {0x0ee2, 0x11b6},    // Hangul_J_RieulHieuh
    {0x0ee3, 0x11b7},    // Hangul_J_Mieum
    {0x0ee4, 0x11b8},    // Hangul_J_Pieub
    {0x0ee5, 0x11b9},    // Hangul_J_PieubSios
    {0x0ee6, 0x11ba},    // Hangul_J_Sios
    {0x0ee7, 0x11bb},    // Hangul_J_SsangSios
    {0x0ee8, 0x11bc},    // Hangul_J_Ieung
    {0x0ee9, 0x11bd},    // Hangul_J_Jieuj
    {0x0eea, 0x11be},    // Hangul_J_Cieuc
    {0x0eeb, 0x11bf},    // Hangul_J_Khieuq
    {0x0eec, 0x11c0},    // Hangul_J_Tieut
    {0x0eed, 0x11c1},    // Hangul_J_Phieuf
    {0x0eee, 0x11c2},    // Hangul_J_Hieuh
    {0x0eef, 0x316d},    // Hangul_RieulYeorinHieuh
    {0x0ef0, 0x3171},    // Hangul_SunkyeongeumMieum
    {0x0ef1, 0x3178},    // Hangul_SunkyeongeumPieub
    {0x0ef2, 0x317f},    // Hangul_PanSios
    {0x0ef3, 0x3181},    // Hangul_KkogjiDalrinIeung
    {0x0ef4, 0x3184},    // Hangul_SunkyeongeumPhieuf
    {0x0ef5, 0x3186},    // Hangul_YeorinHieuh
    {0x0ef6, 0x318d},    // Hangul_AraeA
    {0x0ef7, 0x318e},    // Hangul_AraeAE
    {0x0ef8, 0x11eb},    // Hangul_J_PanSios
    {0x0ef9, 0x11f0},    // Hangul_J_KkogjiDalrinIeung
    {0x0efa, 0x11f9},    // Hangul_J_YeorinHieuh
    {0x0eff, 0x20a9},    // Korean_Won
    {0x13bc, 0x0152},    // OE
    {0x13bd, 0x0153},    // oe
    {0x13be, 0x0178},    // Ydiaeresis
    {0x20a0, 0x20a0},    // EcuSign
    {0x20a1, 0x20a1},    // ColonSign
    {0x20a2, 0x20a2},    // CruzeiroSign
    {0x20a3, 0x20a3},    // FFrancSign
    {0x20a4, 0x20a4},    // LiraSign
    {0x20a5, 0x20a5},    // MillSign
    {0x20a6, 0x20a6},    // NairaSign
    {0x20a7, 0x20a7},    // PesetaSign
    {0x20a8, 0x20a8},    // RupeeSign
    {0x20a9, 0x20a9},    // WonSign
    {0x20aa, 0x20aa},    // NewSheqelSign
    {0x20ab, 0x20ab},    // DongSign
    {0x20ac, 0x20ac},    // EuroSign
    {0xfd01, 0x0000},    // 3270_Duplicate
    {0xfd02, 0x0000},    // 3270_FieldMark
    {0xfd03, 0x0000},    // 3270_Right2
    {0xfd04, 0x0000},    // 3270_Left2
    {0xfd05, 0x0000},    // 3270_BackTab
    {0xfd06, 0x0000},    // 3270_EraseEOF
    {0xfd07, 0x0000},    // 3270_EraseInput
    {0xfd08, 0x0000},    // 3270_Reset
    {0xfd09, 0x0000},    // 3270_Quit
    {0xfd0a, 0x0000},    // 3270_PA1
    {0xfd0b, 0x0000},    // 3270_PA2
    {0xfd0c, 0x0000},    // 3270_PA3
    {0xfd0d, 0x0000},    // 3270_Test
    {0xfd0e, 0x0000},    // 3270_Attn
    {0xfd0f, 0x0000},    // 3270_CursorBlink
    {0xfd10, 0x0000},    // 3270_AltCursor
    {0xfd11, 0x0000},    // 3270_KeyClick
    {0xfd12, 0x0000},    // 3270_Jump
    {0xfd13, 0x0000},    // 3270_Ident
    {0xfd14, 0x0000},    // 3270_Rule
    {0xfd15, 0x0000},    // 3270_Copy
    {0xfd16, 0x0000},    // 3270_Play
    {0xfd17, 0x0000},    // 3270_Setup
    {0xfd18, 0x0000},    // 3270_Record
    {0xfd19, 0x0000},    // 3270_ChangeScreen
    {0xfd1a, 0x0000},    // 3270_DeleteWord
    {0xfd1b, 0x0000},    // 3270_ExSelect
    {0xfd1c, 0x0000},    // 3270_CursorSelect
    {0xfd1d, 0x0000},    // 3270_PrintScreen
    {0xfd1e, 0x0000},    // 3270_Enter
    {0xfe01, 0x0000},    // ISO_Lock
    {0xfe02, 0x0000},    // ISO_Level2_Latch
    {0xfe03, 0x0000},    // ISO_Level3_Shift
    {0xfe04, 0x0000},    // ISO_Level3_Latch
    {0xfe05, 0x0000},    // ISO_Level3_Lock
    {0xfe06, 0x0000},    // ISO_Group_Latch
    {0xfe07, 0x0000},    // ISO_Group_Lock
    {0xfe08, 0x0000},    // ISO_Next_Group
    {0xfe09, 0x0000},    // ISO_Next_Group_Lock
    {0xfe0a, 0x0000},    // ISO_Prev_Group
    {0xfe0b, 0x0000},    // ISO_Prev_Group_Lock
    {0xfe0c, 0x0000},    // ISO_First_Group
    {0xfe0d, 0x0000},    // ISO_First_Group_Lock
    {0xfe0e, 0x0000},    // ISO_Last_Group
    {0xfe0f, 0x0000},    // ISO_Last_Group_Lock
    {0xfe20, 0x0000},    // ISO_Left_Tab
    {0xfe21, 0x0000},    // ISO_Move_Line_Up
    {0xfe22, 0x0000},    // ISO_Move_Line_Down
    {0xfe23, 0x0000},    // ISO_Partial_Line_Up
    {0xfe24, 0x0000},    // ISO_Partial_Line_Down
    {0xfe25, 0x0000},    // ISO_Partial_Space_Left
    {0xfe26, 0x0000},    // ISO_Partial_Space_Right
    {0xfe27, 0x0000},    // ISO_Set_Margin_Left
    {0xfe28, 0x0000},    // ISO_Set_Margin_Right
    {0xfe29, 0x0000},    // ISO_Release_Margin_Left
    {0xfe2a, 0x0000},    // ISO_Release_Margin_Right
    {0xfe2b, 0x0000},    // ISO_Release_Both_Margins
    {0xfe2c, 0x0000},    // ISO_Fast_Cursor_Left
    {0xfe2d, 0x0000},    // ISO_Fast_Cursor_Right
    {0xfe2e, 0x0000},    // ISO_Fast_Cursor_Up
    {0xfe2f, 0x0000},    // ISO_Fast_Cursor_Down
    {0xfe30, 0x0000},    // ISO_Continuous_Underline
    {0xfe31, 0x0000},    // ISO_Discontinuous_Underline
    {0xfe32, 0x0000},    // ISO_Emphasize
    {0xfe33, 0x0000},    // ISO_Center_Object
    {0xfe34, 0x0000},    // ISO_Enter
    {0xfe50, 0x0300},    // dead_grave
    {0xfe51, 0x0301},    // dead_acute
    {0xfe52, 0x0302},    // dead_circumflex
    {0xfe53, 0x0303},    // dead_tilde
    {0xfe54, 0x0304},    // dead_macron
    {0xfe55, 0x0306},    // dead_breve
    {0xfe56, 0x0307},    // dead_abovedot
    {0xfe57, 0x0308},    // dead_diaeresis
    {0xfe58, 0x030a},    // dead_abovering
    {0xfe59, 0x030b},    // dead_doubleacute
    {0xfe5a, 0x030c},    // dead_caron
    {0xfe5b, 0x0327},    // dead_cedilla
    {0xfe5c, 0x0328},    // dead_ogonek
    {0xfe5d, 0x0345},    // dead_iota
    {0xfe5e, 0x3099},    // dead_voiced_sound
    {0xfe5f, 0x309a},    // dead_semivoiced_sound
    {0xfe70, 0x0000},    // AccessX_Enable
    {0xfe71, 0x0000},    // AccessX_Feedback_Enable
    {0xfe72, 0x0000},    // RepeatKeys_Enable
    {0xfe73, 0x0000},    // SlowKeys_Enable
    {0xfe74, 0x0000},    // BounceKeys_Enable
    {0xfe75, 0x0000},    // StickyKeys_Enable
    {0xfe76, 0x0000},    // MouseKeys_Enable
    {0xfe77, 0x0000},    // MouseKeys_Accel_Enable
    {0xfe78, 0x0000},    // Overlay1_Enable
    {0xfe79, 0x0000},    // Overlay2_Enable
    {0xfe7a, 0x0000},    // AudibleBell_Enable
    {0xfed0, 0x0000},    // First_Virtual_Screen
    {0xfed1, 0x0000},    // Prev_Virtual_Screen
    {0xfed2, 0x0000},    // Next_Virtual_Screen
    {0xfed4, 0x0000},    // Last_Virtual_Screen
    {0xfed5, 0x0000},    // Terminate_Server
    {0xfee0, 0x0000},    // Pointer_Left
    {0xfee1, 0x0000},    // Pointer_Right
    {0xfee2, 0x0000},    // Pointer_Up
    {0xfee3, 0x0000},    // Pointer_Down
    {0xfee4, 0x0000},    // Pointer_UpLeft
    {0xfee5, 0x0000},    // Pointer_UpRight
    {0xfee6, 0x0000},    // Pointer_DownLeft
    {0xfee7, 0x0000},    // Pointer_DownRight
    {0xfee8, 0x0000},    // Pointer_Button_Dflt
    {0xfee9, 0x0000},    // Pointer_Button1
    {0xfeea, 0x0000},    // Pointer_Button2
    {0xfeeb, 0x0000},    // Pointer_Button3
    {0xfeec, 0x0000},    // Pointer_Button4
    {0xfeed, 0x0000},    // Pointer_Button5
    {0xfeee, 0x0000},    // Pointer_DblClick_Dflt
    {0xfeef, 0x0000},    // Pointer_DblClick1
    {0xfef0, 0x0000},    // Pointer_DblClick2
    {0xfef1, 0x0000},    // Pointer_DblClick3
    {0xfef2, 0x0000},    // Pointer_DblClick4
    {0xfef3, 0x0000},    // Pointer_DblClick5
    {0xfef4, 0x0000},    // Pointer_Drag_Dflt
    {0xfef5, 0x0000},    // Pointer_Drag1
    {0xfef6, 0x0000},    // Pointer_Drag2
    {0xfef7, 0x0000},    // Pointer_Drag3
    {0xfef8, 0x0000},    // Pointer_Drag4
    {0xfef9, 0x0000},    // Pointer_EnableKeys
    {0xfefa, 0x0000},    // Pointer_Accelerate
    {0xfefb, 0x0000},    // Pointer_DfltBtnNext
    {0xfefc, 0x0000},    // Pointer_DfltBtnPrev
    {0xfefd, 0x0000},    // Pointer_Drag5
    {0xff08, 0x0008},    // BackSpace /* back space, back char */
    {0xff09, 0x0009},    // Tab
    {0xff0a, 0x000a},    // Linefeed /* Linefeed, LF */
    {0xff0b, 0x000b},    // Clear
    {0xff0d, 0x000d},    // Return /* Return, enter */
    {0xff13, 0x0013},    // Pause /* Pause, hold */
    {0xff14, 0x0014},    // Scroll_Lock
    {0xff15, 0x0015},    // Sys_Req
    {0xff1b, 0x001b},    // Escape
    {0xff20, 0x0000},    // Multi_key
    {0xff21, 0x0000},    // Kanji
    {0xff22, 0x0000},    // Muhenkan
    {0xff23, 0x0000},    // Henkan_Mode
    {0xff24, 0x0000},    // Romaji
    {0xff25, 0x0000},    // Hiragana
    {0xff26, 0x0000},    // Katakana
    {0xff27, 0x0000},    // Hiragana_Katakana
    {0xff28, 0x0000},    // Zenkaku
    {0xff29, 0x0000},    // Hankaku
    {0xff2a, 0x0000},    // Zenkaku_Hankaku
    {0xff2b, 0x0000},    // Touroku
    {0xff2c, 0x0000},    // Massyo
    {0xff2d, 0x0000},    // Kana_Lock
    {0xff2e, 0x0000},    // Kana_Shift
    {0xff2f, 0x0000},    // Eisu_Shift
    {0xff30, 0x0000},    // Eisu_toggle
    {0xff31, 0x0000},    // Hangul
    {0xff32, 0x0000},    // Hangul_Start
    {0xff33, 0x0000},    // Hangul_End
    {0xff34, 0x0000},    // Hangul_Hanja
    {0xff35, 0x0000},    // Hangul_Jamo
    {0xff36, 0x0000},    // Hangul_Romaja
    {0xff37, 0x0000},    // Codeinput
    {0xff38, 0x0000},    // Hangul_Jeonja
    {0xff39, 0x0000},    // Hangul_Banja
    {0xff3a, 0x0000},    // Hangul_PreHanja
    {0xff3b, 0x0000},    // Hangul_PostHanja
    {0xff3c, 0x0000},    // SingleCandidate
    {0xff3d, 0x0000},    // MultipleCandidate
    {0xff3e, 0x0000},    // PreviousCandidate
    {0xff3f, 0x0000},    // Hangul_Special
    {0xff50, 0x0000},    // Home
    {0xff51, 0x0000},    // Left
    {0xff52, 0x0000},    // Up
    {0xff53, 0x0000},    // Right
    {0xff54, 0x0000},    // Down
    {0xff55, 0x0000},    // Prior
    {0xff56, 0x0000},    // Next
    {0xff57, 0x0000},    // End
    {0xff58, 0x0000},    // Begin
    {0xff60, 0x0000},    // Select
    {0xff61, 0x0000},    // Print
    {0xff62, 0x0000},    // Execute
    {0xff63, 0x0000},    // Insert
    {0xff65, 0x0000},    // Undo
    {0xff66, 0x0000},    // Redo
    {0xff67, 0x0000},    // Menu
    {0xff68, 0x0000},    // Find
    {0xff69, 0x0000},    // Cancel
    {0xff6a, 0x0000},    // Help
    {0xff6b, 0x0000},    // Break
    {0xff7e, 0x0000},    // Mode_switch
    {0xff7f, 0x0000},    // Num_Lock
    {0xff80, 0x0020},    // KP_Space /* space */
    {0xff89, 0x0009},    // KP_Tab
    {0xff8d, 0x000d},    // KP_Enter /* enter */
    {0xff91, 0x0000},    // KP_F1
    {0xff92, 0x0000},    // KP_F2
    {0xff93, 0x0000},    // KP_F3
    {0xff94, 0x0000},    // KP_F4
    {0xff95, 0x0000},    // KP_Home
    {0xff96, 0x0000},    // KP_Left
    {0xff97, 0x0000},    // KP_Up
    {0xff98, 0x0000},    // KP_Right
    {0xff99, 0x0000},    // KP_Down
    {0xff9a, 0x0000},    // KP_Prior
    {0xff9b, 0x0000},    // KP_Next
    {0xff9c, 0x0000},    // KP_End
    {0xff9d, 0x0000},    // KP_Begin
    {0xff9e, 0x0000},    // KP_Insert
    {0xff9f, 0x0000},    // KP_Delete
    {0xffaa, 0x002a},    // KP_Multiply
    {0xffab, 0x002b},    // KP_Add
    {0xffac, 0x002c},    // KP_Separator /* separator, often comma */
    {0xffad, 0x002d},    // KP_Subtract
    {0xffae, 0x002e},    // KP_Decimal
    {0xffaf, 0x002f},    // KP_Divide
    {0xffb0, 0x0030},    // KP_0
    {0xffb1, 0x0031},    // KP_1
    {0xffb2, 0x0032},    // KP_2
    {0xffb3, 0x0033},    // KP_3
    {0xffb4, 0x0034},    // KP_4
    {0xffb5, 0x0035},    // KP_5
    {0xffb6, 0x0036},    // KP_6
    {0xffb7, 0x0037},    // KP_7
    {0xffb8, 0x0038},    // KP_8
    {0xffb9, 0x0039},    // KP_9
    {0xffbd, 0x003d},    // KP_Equal /* equals */
    {0xffbe, 0x0000},    // F1
    {0xffbf, 0x0000},    // F2
    {0xffc0, 0x0000},    // F3
    {0xffc1, 0x0000},    // F4
    {0xffc2, 0x0000},    // F5
    {0xffc3, 0x0000},    // F6
    {0xffc4, 0x0000},    // F7
    {0xffc5, 0x0000},    // F8
    {0xffc6, 0x0000},    // F9
    {0xffc7, 0x0000},    // F10
    {0xffc8, 0x0000},    // F11
    {0xffc9, 0x0000},    // F12
    {0xffca, 0x0000},    // F13
    {0xffcb, 0x0000},    // F14
    {0xffcc, 0x0000},    // F15
    {0xffcd, 0x0000},    // F16
    {0xffce, 0x0000},    // F17
    {0xffcf, 0x0000},    // F18
    {0xffd0, 0x0000},    // F19
    {0xffd1, 0x0000},    // F20
    {0xffd2, 0x0000},    // F21
    {0xffd3, 0x0000},    // F22
    {0xffd4, 0x0000},    // F23
    {0xffd5, 0x0000},    // F24
    {0xffd6, 0x0000},    // F25
    {0xffd7, 0x0000},    // F26
    {0xffd8, 0x0000},    // F27
    {0xffd9, 0x0000},    // F28
    {0xffda, 0x0000},    // F29
    {0xffdb, 0x0000},    // F30
    {0xffdc, 0x0000},    // F31
    {0xffdd, 0x0000},    // F32
    {0xffde, 0x0000},    // F33
    {0xffdf, 0x0000},    // F34
    {0xffe0, 0x0000},    // F35
    {0xffe1, 0x0000},    // Shift_L
    {0xffe2, 0x0000},    // Shift_R
    {0xffe3, 0x0000},    // Control_L
    {0xffe4, 0x0000},    // Control_R
    {0xffe5, 0x0000},    // Caps_Lock
    {0xffe6, 0x0000},    // Shift_Lock
    {0xffe7, 0x0000},    // Meta_L
    {0xffe8, 0x0000},    // Meta_R
    {0xffe9, 0x0000},    // Alt_L
    {0xffea, 0x0000},    // Alt_R
    {0xffeb, 0x0000},    // Super_L
    {0xffec, 0x0000},    // Super_R
    {0xffed, 0x0000},    // Hyper_L
    {0xffee, 0x0000},    // Hyper_R
    {0xffff, 0x0000},    // Delete
    {0xffffff, 0x0000},    // VoidSymbol

    // Various XFree86 extensions since X11R6.4
    // http://cvsweb.xfree86.org/cvsweb/xc/include/keysymdef.h

    // KOI8-U support (Aleksey Novodvorsky, 1999-05-30)
    // http://cvsweb.xfree86.org/cvsweb/xc/include/keysymdef.h.diff?r1=1.4&r2=1.5
    // Used in XFree86's /usr/lib/X11/xkb/symbols/ua mappings

    {0x06ad, 0x0491},    // Ukrainian_ghe_with_upturn
    {0x06bd, 0x0490},    // Ukrainian_GHE_WITH_UPTURN

    // Support for armscii-8, ibm-cp1133, mulelao-1, viscii1.1-1,
    // tcvn-5712, georgian-academy, georgian-ps
    // (#2843, Pablo Saratxaga <pablo@mandrakesoft.com>, 1999-06-06)
    // http://cvsweb.xfree86.org/cvsweb/xc/include/keysymdef.h.diff?r1=1.6&r2=1.7

    // Armenian
    // (not used in any XFree86 4.4 kbd layouts, where /usr/lib/X11/xkb/symbols/am
    // uses directly Unicode-mapped hexadecimal values instead)
    {0x14a1, 0x0000},    // Armenian_eternity
    {0x14a2, 0x0587},    // Armenian_ligature_ew
    {0x14a3, 0x0589},    // Armenian_verjaket
    {0x14a4, 0x0029},    // Armenian_parenright
    {0x14a5, 0x0028},    // Armenian_parenleft
    {0x14a6, 0x00bb},    // Armenian_guillemotright
    {0x14a7, 0x00ab},    // Armenian_guillemotleft
    {0x14a8, 0x2014},    // Armenian_em_dash
    {0x14a9, 0x002e},    // Armenian_mijaket
    {0x14aa, 0x055d},    // Armenian_but
    {0x14ab, 0x002c},    // Armenian_comma
    {0x14ac, 0x2013},    // Armenian_en_dash
    {0x14ad, 0x058a},    // Armenian_yentamna
    {0x14ae, 0x2026},    // Armenian_ellipsis
    {0x14af, 0x055c},    // Armenian_amanak
    {0x14b0, 0x055b},    // Armenian_shesht
    {0x14b1, 0x055e},    // Armenian_paruyk
    {0x14b2, 0x0531},    // Armenian_AYB
    {0x14b3, 0x0561},    // Armenian_ayb
    {0x14b4, 0x0532},    // Armenian_BEN
    {0x14b5, 0x0562},    // Armenian_ben
    {0x14b6, 0x0533},    // Armenian_GIM
    {0x14b7, 0x0563},    // Armenian_gim
    {0x14b8, 0x0534},    // Armenian_DA
    {0x14b9, 0x0564},    // Armenian_da
    {0x14ba, 0x0535},    // Armenian_YECH
    {0x14bb, 0x0565},    // Armenian_yech
    {0x14bc, 0x0536},    // Armenian_ZA
    {0x14bd, 0x0566},    // Armenian_za
    {0x14be, 0x0537},    // Armenian_E
    {0x14bf, 0x0567},    // Armenian_e
    {0x14c0, 0x0538},    // Armenian_AT
    {0x14c1, 0x0568},    // Armenian_at
    {0x14c2, 0x0539},    // Armenian_TO
    {0x14c3, 0x0569},    // Armenian_to
    {0x14c4, 0x053a},    // Armenian_ZHE
    {0x14c5, 0x056a},    // Armenian_zhe
    {0x14c6, 0x053b},    // Armenian_INI
    {0x14c7, 0x056b},    // Armenian_ini
    {0x14c8, 0x053c},    // Armenian_LYUN
    {0x14c9, 0x056c},    // Armenian_lyun
    {0x14ca, 0x053d},    // Armenian_KHE
    {0x14cb, 0x056d},    // Armenian_khe
    {0x14cc, 0x053e},    // Armenian_TSA
    {0x14cd, 0x056e},    // Armenian_tsa
    {0x14ce, 0x053f},    // Armenian_KEN
    {0x14cf, 0x056f},    // Armenian_ken
    {0x14d0, 0x0540},    // Armenian_HO
    {0x14d1, 0x0570},    // Armenian_ho
    {0x14d2, 0x0541},    // Armenian_DZA
    {0x14d3, 0x0571},    // Armenian_dza
    {0x14d4, 0x0542},    // Armenian_GHAT
    {0x14d5, 0x0572},    // Armenian_ghat
    {0x14d6, 0x0543},    // Armenian_TCHE
    {0x14d7, 0x0573},    // Armenian_tche
    {0x14d8, 0x0544},    // Armenian_MEN
    {0x14d9, 0x0574},    // Armenian_men
    {0x14da, 0x0545},    // Armenian_HI
    {0x14db, 0x0575},    // Armenian_hi
    {0x14dc, 0x0546},    // Armenian_NU
    {0x14dd, 0x0576},    // Armenian_nu
    {0x14de, 0x0547},    // Armenian_SHA
    {0x14df, 0x0577},    // Armenian_sha
    {0x14e0, 0x0548},    // Armenian_VO
    {0x14e1, 0x0578},    // Armenian_vo
    {0x14e2, 0x0549},    // Armenian_CHA
    {0x14e3, 0x0579},    // Armenian_cha
    {0x14e4, 0x054a},    // Armenian_PE
    {0x14e5, 0x057a},    // Armenian_pe
    {0x14e6, 0x054b},    // Armenian_JE
    {0x14e7, 0x057b},    // Armenian_je
    {0x14e8, 0x054c},    // Armenian_RA
    {0x14e9, 0x057c},    // Armenian_ra
    {0x14ea, 0x054d},    // Armenian_SE
    {0x14eb, 0x057d},    // Armenian_se
    {0x14ec, 0x054e},    // Armenian_VEV
    {0x14ed, 0x057e},    // Armenian_vev
    {0x14ee, 0x054f},    // Armenian_TYUN
    {0x14ef, 0x057f},    // Armenian_tyun
    {0x14f0, 0x0550},    // Armenian_RE
    {0x14f1, 0x0580},    // Armenian_re
    {0x14f2, 0x0551},    // Armenian_TSO
    {0x14f3, 0x0581},    // Armenian_tso
    {0x14f4, 0x0552},    // Armenian_VYUN
    {0x14f5, 0x0582},    // Armenian_vyun
    {0x14f6, 0x0553},    // Armenian_PYUR
    {0x14f7, 0x0583},    // Armenian_pyur
    {0x14f8, 0x0554},    // Armenian_KE
    {0x14f9, 0x0584},    // Armenian_ke
    {0x14fa, 0x0555},    // Armenian_O
    {0x14fb, 0x0585},    // Armenian_o
    {0x14fc, 0x0556},    // Armenian_FE
    {0x14fd, 0x0586},    // Armenian_fe
    {0x14fe, 0x055a},    // Armenian_apostrophe
    {0x14ff, 0x00a7},    // Armenian_section_sign

    // Gregorian
    // (not used in any XFree86 4.4 kbd layouts, were /usr/lib/X11/xkb/symbols/ge_*
    // uses directly Unicode-mapped hexadecimal values instead)
    {0x15d0, 0x10d0},    // Georgian_an
    {0x15d1, 0x10d1},    // Georgian_ban
    {0x15d2, 0x10d2},    // Georgian_gan
    {0x15d3, 0x10d3},    // Georgian_don
    {0x15d4, 0x10d4},    // Georgian_en
    {0x15d5, 0x10d5},    // Georgian_vin
    {0x15d6, 0x10d6},    // Georgian_zen
    {0x15d7, 0x10d7},    // Georgian_tan
    {0x15d8, 0x10d8},    // Georgian_in
    {0x15d9, 0x10d9},    // Georgian_kan
    {0x15da, 0x10da},    // Georgian_las
    {0x15db, 0x10db},    // Georgian_man
    {0x15dc, 0x10dc},    // Georgian_nar
    {0x15dd, 0x10dd},    // Georgian_on
    {0x15de, 0x10de},    // Georgian_par
    {0x15df, 0x10df},    // Georgian_zhar
    {0x15e0, 0x10e0},    // Georgian_rae
    {0x15e1, 0x10e1},    // Georgian_san
    {0x15e2, 0x10e2},    // Georgian_tar
    {0x15e3, 0x10e3},    // Georgian_un
    {0x15e4, 0x10e4},    // Georgian_phar
    {0x15e5, 0x10e5},    // Georgian_khar
    {0x15e6, 0x10e6},    // Georgian_ghan
    {0x15e7, 0x10e7},    // Georgian_qar
    {0x15e8, 0x10e8},    // Georgian_shin
    {0x15e9, 0x10e9},    // Georgian_chin
    {0x15ea, 0x10ea},    // Georgian_can
    {0x15eb, 0x10eb},    // Georgian_jil
    {0x15ec, 0x10ec},    // Georgian_cil
    {0x15ed, 0x10ed},    // Georgian_char
    {0x15ee, 0x10ee},    // Georgian_xan
    {0x15ef, 0x10ef},    // Georgian_jhan
    {0x15f0, 0x10f0},    // Georgian_hae
    {0x15f1, 0x10f1},    // Georgian_he
    {0x15f2, 0x10f2},    // Georgian_hie
    {0x15f3, 0x10f3},    // Georgian_we
    {0x15f4, 0x10f4},    // Georgian_har
    {0x15f5, 0x10f5},    // Georgian_hoe
    {0x15f6, 0x10f6},    // Georgian_fi

    // Pablo Saratxaga's i18n updates for XFree86 that are used in Mandrake 7.2.
    // (#4195, Pablo Saratxaga <pablo@mandrakesoft.com>, 2000-10-27)
    // http://cvsweb.xfree86.org/cvsweb/xc/include/keysymdef.h.diff?r1=1.9&r2=1.10

    // Latin-8
    // (the *abovedot keysyms are used in /usr/lib/X11/xkb/symbols/ie)
    {0x12a1, 0x1e02},    // Babovedot
    {0x12a2, 0x1e03},    // babovedot
    {0x12a6, 0x1e0a},    // Dabovedot
    {0x12a8, 0x1e80},    // Wgrave
    {0x12aa, 0x1e82},    // Wacute
    {0x12ab, 0x1e0b},    // dabovedot
    {0x12ac, 0x1ef2},    // Ygrave
    {0x12b0, 0x1e1e},    // Fabovedot
    {0x12b1, 0x1e1f},    // fabovedot
    {0x12b4, 0x1e40},    // Mabovedot
    {0x12b5, 0x1e41},    // mabovedot
    {0x12b7, 0x1e56},    // Pabovedot
    {0x12b8, 0x1e81},    // wgrave
    {0x12b9, 0x1e57},    // pabovedot
    {0x12ba, 0x1e83},    // wacute
    {0x12bb, 0x1e60},    // Sabovedot
    {0x12bc, 0x1ef3},    // ygrave
    {0x12bd, 0x1e84},    // Wdiaeresis
    {0x12be, 0x1e85},    // wdiaeresis
    {0x12bf, 0x1e61},    // sabovedot
    {0x12d0, 0x0174},    // Wcircumflex
    {0x12d7, 0x1e6a},    // Tabovedot
    {0x12de, 0x0176},    // Ycircumflex
    {0x12f0, 0x0175},    // wcircumflex
    {0x12f7, 0x1e6b},    // tabovedot
    {0x12fe, 0x0177},    // ycircumflex

    // Arabic
    // (of these, in XFree86 4.4 only Arabic_superscript_alef, Arabic_madda_above,
    // Arabic_hamza_* are actually used, e.g. in /usr/lib/X11/xkb/symbols/syr)
    {0x0590, 0x06f0},    // Farsi_0
    {0x0591, 0x06f1},    // Farsi_1
    {0x0592, 0x06f2},    // Farsi_2
    {0x0593, 0x06f3},    // Farsi_3
    {0x0594, 0x06f4},    // Farsi_4
    {0x0595, 0x06f5},    // Farsi_5
    {0x0596, 0x06f6},    // Farsi_6
    {0x0597, 0x06f7},    // Farsi_7
    {0x0598, 0x06f8},    // Farsi_8
    {0x0599, 0x06f9},    // Farsi_9
    {0x05a5, 0x066a},    // Arabic_percent
    {0x05a6, 0x0670},    // Arabic_superscript_alef
    {0x05a7, 0x0679},    // Arabic_tteh
    {0x05a8, 0x067e},    // Arabic_peh
    {0x05a9, 0x0686},    // Arabic_tcheh
    {0x05aa, 0x0688},    // Arabic_ddal
    {0x05ab, 0x0691},    // Arabic_rreh
    {0x05ae, 0x06d4},    // Arabic_fullstop
    {0x05b0, 0x0660},    // Arabic_0
    {0x05b1, 0x0661},    // Arabic_1
    {0x05b2, 0x0662},    // Arabic_2
    {0x05b3, 0x0663},    // Arabic_3
    {0x05b4, 0x0664},    // Arabic_4
    {0x05b5, 0x0665},    // Arabic_5
    {0x05b6, 0x0666},    // Arabic_6
    {0x05b7, 0x0667},    // Arabic_7
    {0x05b8, 0x0668},    // Arabic_8
    {0x05b9, 0x0669},    // Arabic_9
    {0x05f3, 0x0653},    // Arabic_madda_above
    {0x05f4, 0x0654},    // Arabic_hamza_above
    {0x05f5, 0x0655},    // Arabic_hamza_below
    {0x05f6, 0x0698},    // Arabic_jeh
    {0x05f7, 0x06a4},    // Arabic_veh
    {0x05f8, 0x06a9},    // Arabic_keheh
    {0x05f9, 0x06af},    // Arabic_gaf
    {0x05fa, 0x06ba},    // Arabic_noon_ghunna
    {0x05fb, 0x06be},    // Arabic_heh_doachashmee
    {0x05fc, 0x06cc},    // Farsi_yeh
    {0x05fd, 0x06d2},    // Arabic_yeh_baree
    {0x05fe, 0x06c1},    // Arabic_heh_goal

    // Cyrillic
    // (none of these are actually used in any XFree86 4.4 kbd layouts)
    {0x0680, 0x0492},    // Cyrillic_GHE_bar
    {0x0681, 0x0496},    // Cyrillic_ZHE_descender
    {0x0682, 0x049a},    // Cyrillic_KA_descender
    {0x0683, 0x049c},    // Cyrillic_KA_vertstroke
    {0x0684, 0x04a2},    // Cyrillic_EN_descender
    {0x0685, 0x04ae},    // Cyrillic_U_straight
    {0x0686, 0x04b0},    // Cyrillic_U_straight_bar
    {0x0687, 0x04b2},    // Cyrillic_HA_descender
    {0x0688, 0x04b6},    // Cyrillic_CHE_descender
    {0x0689, 0x04b8},    // Cyrillic_CHE_vertstroke
    {0x068a, 0x04ba},    // Cyrillic_SHHA
    {0x068c, 0x04d8},    // Cyrillic_SCHWA
    {0x068d, 0x04e2},    // Cyrillic_I_macron
    {0x068e, 0x04e8},    // Cyrillic_O_bar
    {0x068f, 0x04ee},    // Cyrillic_U_macron
    {0x0690, 0x0493},    // Cyrillic_ghe_bar
    {0x0691, 0x0497},    // Cyrillic_zhe_descender
    {0x0692, 0x049b},    // Cyrillic_ka_descender
    {0x0693, 0x049d},    // Cyrillic_ka_vertstroke
    {0x0694, 0x04a3},    // Cyrillic_en_descender
    {0x0695, 0x04af},    // Cyrillic_u_straight
    {0x0696, 0x04b1},    // Cyrillic_u_straight_bar
    {0x0697, 0x04b3},    // Cyrillic_ha_descender
    {0x0698, 0x04b7},    // Cyrillic_che_descender
    {0x0699, 0x04b9},    // Cyrillic_che_vertstroke
    {0x069a, 0x04bb},    // Cyrillic_shha
    {0x069c, 0x04d9},    // Cyrillic_schwa
    {0x069d, 0x04e3},    // Cyrillic_i_macron
    {0x069e, 0x04e9},    // Cyrillic_o_bar
    {0x069f, 0x04ef},    // Cyrillic_u_macron

    // Caucasus
    // (of these, in XFree86 4.4 only Gcaron, gcaron are actually used,
    // e.g. in /usr/lib/X11/xkb/symbols/sapmi; the lack of Unicode
    // equivalents for the others suggests that they are bogus)
    {0x16a2, 0x0000},    // Ccedillaabovedot
    {0x16a3, 0x1e8a},    // Xabovedot
    {0x16a5, 0x0000},    // Qabovedot
    {0x16a6, 0x012c},    // Ibreve
    {0x16a7, 0x0000},    // IE
    {0x16a8, 0x0000},    // UO
    {0x16a9, 0x01b5},    // Zstroke
    {0x16aa, 0x01e6},    // Gcaron
    {0x16af, 0x019f},    // Obarred
    {0x16b2, 0x0000},    // ccedillaabovedot
    {0x16b3, 0x1e8b},    // xabovedot
    {0x16b4, 0x0000},    // Ocaron
    {0x16b5, 0x0000},    // qabovedot
    {0x16b6, 0x012d},    // ibreve
    {0x16b7, 0x0000},    // ie
    {0x16b8, 0x0000},    // uo
    {0x16b9, 0x01b6},    // zstroke
    {0x16ba, 0x01e7},    // gcaron
    {0x16bd, 0x01d2},    // ocaron
    {0x16bf, 0x0275},    // obarred
    {0x16c6, 0x018f},    // SCHWA
    {0x16f6, 0x0259},    // schwa

    // Inupiak, Guarani
    // (none of these are actually used in any XFree86 4.4 kbd layouts,
    // and the lack of Unicode equivalents suggests that they are bogus)
    {0x16d1, 0x1e36},    // Lbelowdot
    {0x16d2, 0x0000},    // Lstrokebelowdot
    {0x16d3, 0x0000},    // Gtilde
    {0x16e1, 0x1e37},    // lbelowdot
    {0x16e2, 0x0000},    // lstrokebelowdot
    {0x16e3, 0x0000},    // gtilde

    // Vietnamese
    // (none of these are actually used in any XFree86 4.4 kbd layouts; they are
    // also pointless, as Vietnamese input methods use dead accent keys + ASCII keys)
    {0x1ea0, 0x1ea0},    // Abelowdot
    {0x1ea1, 0x1ea1},    // abelowdot
    {0x1ea2, 0x1ea2},    // Ahook
    {0x1ea3, 0x1ea3},    // ahook
    {0x1ea4, 0x1ea4},    // Acircumflexacute
    {0x1ea5, 0x1ea5},    // acircumflexacute
    {0x1ea6, 0x1ea6},    // Acircumflexgrave
    {0x1ea7, 0x1ea7},    // acircumflexgrave
    {0x1ea8, 0x1ea8},    // Acircumflexhook
    {0x1ea9, 0x1ea9},    // acircumflexhook
    {0x1eaa, 0x1eaa},    // Acircumflextilde
    {0x1eab, 0x1eab},    // acircumflextilde
    {0x1eac, 0x1eac},    // Acircumflexbelowdot
    {0x1ead, 0x1ead},    // acircumflexbelowdot
    {0x1eae, 0x1eae},    // Abreveacute
    {0x1eaf, 0x1eaf},    // abreveacute
    {0x1eb0, 0x1eb0},    // Abrevegrave
    {0x1eb1, 0x1eb1},    // abrevegrave
    {0x1eb2, 0x1eb2},    // Abrevehook
    {0x1eb3, 0x1eb3},    // abrevehook
    {0x1eb4, 0x1eb4},    // Abrevetilde
    {0x1eb5, 0x1eb5},    // abrevetilde
    {0x1eb6, 0x1eb6},    // Abrevebelowdot
    {0x1eb7, 0x1eb7},    // abrevebelowdot
    {0x1eb8, 0x1eb8},    // Ebelowdot
    {0x1eb9, 0x1eb9},    // ebelowdot
    {0x1eba, 0x1eba},    // Ehook
    {0x1ebb, 0x1ebb},    // ehook
    {0x1ebc, 0x1ebc},    // Etilde
    {0x1ebd, 0x1ebd},    // etilde
    {0x1ebe, 0x1ebe},    // Ecircumflexacute
    {0x1ebf, 0x1ebf},    // ecircumflexacute
    {0x1ec0, 0x1ec0},    // Ecircumflexgrave
    {0x1ec1, 0x1ec1},    // ecircumflexgrave
    {0x1ec2, 0x1ec2},    // Ecircumflexhook
    {0x1ec3, 0x1ec3},    // ecircumflexhook
    {0x1ec4, 0x1ec4},    // Ecircumflextilde
    {0x1ec5, 0x1ec5},    // ecircumflextilde
    {0x1ec6, 0x1ec6},    // Ecircumflexbelowdot
    {0x1ec7, 0x1ec7},    // ecircumflexbelowdot
    {0x1ec8, 0x1ec8},    // Ihook
    {0x1ec9, 0x1ec9},    // ihook
    {0x1eca, 0x1eca},    // Ibelowdot
    {0x1ecb, 0x1ecb},    // ibelowdot
    {0x1ecc, 0x1ecc},    // Obelowdot
    {0x1ecd, 0x1ecd},    // obelowdot
    {0x1ece, 0x1ece},    // Ohook
    {0x1ecf, 0x1ecf},    // ohook
    {0x1ed0, 0x1ed0},    // Ocircumflexacute
    {0x1ed1, 0x1ed1},    // ocircumflexacute
    {0x1ed2, 0x1ed2},    // Ocircumflexgrave
    {0x1ed3, 0x1ed3},    // ocircumflexgrave
    {0x1ed4, 0x1ed4},    // Ocircumflexhook
    {0x1ed5, 0x1ed5},    // ocircumflexhook
    {0x1ed6, 0x1ed6},    // Ocircumflextilde
    {0x1ed7, 0x1ed7},    // ocircumflextilde
    {0x1ed8, 0x1ed8},    // Ocircumflexbelowdot
    {0x1ed9, 0x1ed9},    // ocircumflexbelowdot
    {0x1eda, 0x1eda},    // Ohornacute
    {0x1edb, 0x1edb},    // ohornacute
    {0x1edc, 0x1edc},    // Ohorngrave
    {0x1edd, 0x1edd},    // ohorngrave
    {0x1ede, 0x1ede},    // Ohornhook
    {0x1edf, 0x1edf},    // ohornhook
    {0x1ee0, 0x1ee0},    // Ohorntilde
    {0x1ee1, 0x1ee1},    // ohorntilde
    {0x1ee2, 0x1ee2},    // Ohornbelowdot
    {0x1ee3, 0x1ee3},    // ohornbelowdot
    {0x1ee4, 0x1ee4},    // Ubelowdot
    {0x1ee5, 0x1ee5},    // ubelowdot
    {0x1ee6, 0x1ee6},    // Uhook
    {0x1ee7, 0x1ee7},    // uhook
    {0x1ee8, 0x1ee8},    // Uhornacute
    {0x1ee9, 0x1ee9},    // uhornacute
    {0x1eea, 0x1eea},    // Uhorngrave
    {0x1eeb, 0x1eeb},    // uhorngrave
    {0x1eec, 0x1eec},    // Uhornhook
    {0x1eed, 0x1eed},    // uhornhook
    {0x1eee, 0x1eee},    // Uhorntilde
    {0x1eef, 0x1eef},    // uhorntilde
    {0x1ef0, 0x1ef0},    // Uhornbelowdot
    {0x1ef1, 0x1ef1},    // uhornbelowdot
    {0x1ef4, 0x1ef4},    // Ybelowdot
    {0x1ef5, 0x1ef5},    // ybelowdot
    {0x1ef6, 0x1ef6},    // Yhook
    {0x1ef7, 0x1ef7},    // yhook
    {0x1ef8, 0x1ef8},    // Ytilde
    {0x1ef9, 0x1ef9},    // ytilde

    {0x1efa, 0x01a0},    // Ohorn
    {0x1efb, 0x01a1},    // ohorn
    {0x1efc, 0x01af},    // Uhorn
    {0x1efd, 0x01b0},    // uhorn

    //# (Unicode combining characters have no direct equivalence with
    //# keysyms, where dead keys are defined instead)
    {0x1e9f, 0x0303},    // combining_tilde
    {0x1ef2, 0x0300},    // combining_grave
    {0x1ef3, 0x0301},    // combining_acute
    {0x1efe, 0x0309},    // combining_hook
    {0x1eff, 0x0323},    // combining_belowdot

    //# These probably should be added to the X11 standard properly,
    //# as they could be of use for Vietnamese input methods.
    {0xfe60, 0x0323},    // dead_belowdot
    {0xfe61, 0x0309},    // dead_hook
    {0xfe62, 0x031b},    // dead_horn
};
int wxUnicodeCharXToWX(WXKeySym keySym)
{
    int id = wxCharCodeXToWX (keySym);

    /* first check for Latin-1 characters (1:1 mapping) */
    if ( id != WXK_NONE )
        return id;

    int min = 0;
    int max = sizeof(keySymTab) / sizeof(CodePair) - 1;
    int mid;

    /* also check for directly encoded 24-bit UCS characters */
    if ( (keySym & 0xff000000) == 0x01000000 )
        return keySym & 0x00ffffff;

    /* binary search in table */
    while ( max >= min ) {
        mid = (min + max) / 2;
        if ( keySymTab[mid].keySym < keySym )
            min = mid + 1;
        else if ( keySymTab[mid].keySym > keySym )
            max = mid - 1;
        else {
            /* found it */
            return keySymTab[mid].uniChar;
        }
    }

    // no matching keycode value found
    return WXK_NONE;
}

// ----------------------------------------------------------------------------
// check current state of a key
// ----------------------------------------------------------------------------

bool wxGetKeyState(wxKeyCode key)
{
    wxASSERT_MSG(key != WXK_LBUTTON && key != WXK_RBUTTON && key !=
        WXK_MBUTTON, wxT("can't use wxGetKeyState() for mouse buttons"));

    Display *pDisplay = (Display*) wxGetDisplay();

    int iKey = wxCharCodeWXToX(key);
    int          iKeyMask = 0;
    Window       wDummy1, wDummy2;
    int          iDummy3, iDummy4, iDummy5, iDummy6;
    unsigned int iMask;
    KeyCode keyCode = XKeysymToKeycode(pDisplay,iKey);
    if (keyCode == NoSymbol)
        return false;

    if ( IsModifierKey(iKey) )  // If iKey is a modifier key, use a different method
    {
        XModifierKeymap *map = XGetModifierMapping(pDisplay);
        wxCHECK_MSG( map, false, wxT("failed to get X11 modifiers map") );

        for (int i = 0; i < 8; ++i)
        {
            if ( map->modifiermap[map->max_keypermod * i] == keyCode)
            {
                iKeyMask = 1 << i;
            }
        }

        XQueryPointer(pDisplay, DefaultRootWindow(pDisplay), &wDummy1, &wDummy2,
                        &iDummy3, &iDummy4, &iDummy5, &iDummy6, &iMask );
        XFreeModifiermap(map);
        return (iMask & iKeyMask) != 0;
    }

    // From the XLib manual:
    // The XQueryKeymap() function returns a bit vector for the logical state of the keyboard,
    // where each bit set to 1 indicates that the corresponding key is currently pressed down.
    // The vector is represented as 32 bytes. Byte N (from 0) contains the bits for keys 8N to 8N + 7
    // with the least-significant bit in the byte representing key 8N.
    char key_vector[32];
    XQueryKeymap(pDisplay, key_vector);
    return key_vector[keyCode >> 3] & (1 << (keyCode & 7));
}

#endif // !defined(__WXGTK__) || defined(GDK_WINDOWING_X11)

// ----------------------------------------------------------------------------
// Launch document with default app
// ----------------------------------------------------------------------------

bool wxLaunchDefaultApplication(const wxString& document, int flags)
{
    wxUnusedVar(flags);

    // Our best best is to use xdg-open from freedesktop.org cross-desktop
    // compatibility suite xdg-utils
    // (see http://portland.freedesktop.org/wiki/) -- this is installed on
    // most modern distributions and may be tweaked by them to handle
    // distribution specifics.
    wxString path, xdg_open;
    if ( wxGetEnv("PATH", &path) &&
         wxFindFileInPath(&xdg_open, path, "xdg-open") )
    {
        if ( wxExecute(xdg_open + " " + document) )
            return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

bool
wxDoLaunchDefaultBrowser(const wxLaunchBrowserParams& params)
{
#ifdef __WXGTK__
#if GTK_CHECK_VERSION(2,14,0)
#ifndef __WXGTK3__
    if (gtk_check_version(2,14,0) == NULL)
#endif
    {
        GdkScreen* screen = gdk_window_get_screen(wxGetTopLevelGDK());
        if (gtk_show_uri(screen, params.url.utf8_str(), GDK_CURRENT_TIME, NULL))
            return true;
    }
#endif // GTK_CHECK_VERSION(2,14,0)
#endif // __WXGTK__

    // Our best best is to use xdg-open from freedesktop.org cross-desktop
    // compatibility suite xdg-utils
    // (see http://portland.freedesktop.org/wiki/) -- this is installed on
    // most modern distributions and may be tweaked by them to handle
    // distribution specifics. Only if that fails, try to find the right
    // browser ourselves.
    wxString path, xdg_open;
    if ( wxGetEnv("PATH", &path) &&
         wxFindFileInPath(&xdg_open, path, "xdg-open") )
    {
        if ( wxExecute(xdg_open + " " + params.GetPathOrURL()) )
            return true;
    }

    wxString desktop = wxTheApp->GetTraits()->GetDesktopEnvironment();

    // GNOME and KDE desktops have some applications which should be always installed
    // together with their main parts, which give us the
    if (desktop == wxT("GNOME"))
    {
        wxArrayString errors;
        wxArrayString output;

        // gconf will tell us the path of the application to use as browser
        long res = wxExecute( wxT("gconftool-2 --get /desktop/gnome/applications/browser/exec"),
                              output, errors, wxEXEC_NODISABLE );
        if (res >= 0 && errors.GetCount() == 0)
        {
            wxString cmd = output[0];
            cmd << wxT(' ') << params.GetPathOrURL();
            if (wxExecute(cmd))
                return true;
        }
    }
    else if (desktop == wxT("KDE"))
    {
        // kfmclient directly opens the given URL
        if (wxExecute(wxT("kfmclient openURL ") + params.GetPathOrURL()))
            return true;
    }

    return false;
}

#endif // __WXX11__ || __WXGTK__ || __WXMOTIF__
