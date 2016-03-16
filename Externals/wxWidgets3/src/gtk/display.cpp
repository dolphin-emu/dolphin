///////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/display.cpp
// Author:      Paul Cornett
// Created:     2014-04-17
// Copyright:   (c) 2014 Paul Cornett
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_DISPLAY
    #include "wx/display.h"
    #include "wx/display_impl.h"
#endif
#include "wx/utils.h" // wxClientDisplayRect

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
    #include <gdk/gdkx.h>
#endif
#include "wx/gtk/private/gtk2-compat.h"

GdkWindow* wxGetTopLevelGDK();

//-----------------------------------------------------------------------------

#ifdef GDK_WINDOWING_X11
void wxGetWorkAreaX11(Screen* screen, int& x, int& y, int& width, int& height);
#endif

#ifndef __WXGTK3__
static inline int wx_gdk_screen_get_primary_monitor(GdkScreen* screen)
{
    int monitor = 0;
#if GTK_CHECK_VERSION(2,20,0)
    if (gtk_check_version(2,20,0) == NULL)
        monitor = gdk_screen_get_primary_monitor(screen);
#endif
    return monitor;
}
#define gdk_screen_get_primary_monitor wx_gdk_screen_get_primary_monitor
#endif // !__WXGTK3__

static inline void
wx_gdk_screen_get_monitor_workarea(GdkScreen* screen, int monitor, GdkRectangle* dest)
{
#if GTK_CHECK_VERSION(3,4,0)
    if (gtk_check_version(3,4,0) == NULL)
        gdk_screen_get_monitor_workarea(screen, monitor, dest);
    else
#endif
    {
        gdk_screen_get_monitor_geometry(screen, monitor, dest);
#ifdef GDK_WINDOWING_X11
#ifdef __WXGTK3__
        if (GDK_IS_X11_SCREEN(screen))
#endif
        {
            GdkRectangle rect = { 0 };
            wxGetWorkAreaX11(GDK_SCREEN_XSCREEN(screen),
                rect.x, rect.y, rect.width, rect.height);
            // in case _NET_WORKAREA result is too large
            if (rect.width && rect.height)
                gdk_rectangle_intersect(dest, &rect, dest);
        }
#endif // GDK_WINDOWING_X11
    }
}
#define gdk_screen_get_monitor_workarea wx_gdk_screen_get_monitor_workarea

void wxClientDisplayRect(int* x, int* y, int* width, int* height)
{
    GdkRectangle rect;
    GdkWindow* window = wxGetTopLevelGDK();
    GdkScreen* screen = gdk_window_get_screen(window);
    int monitor = gdk_screen_get_monitor_at_window(screen, window);
    gdk_screen_get_monitor_workarea(screen, monitor, &rect);
    if (x)
        *x = rect.x;
    if (y)
        *y = rect.y;
    if (width)
        *width = rect.width;
    if (height)
        *height = rect.height;
}
//-----------------------------------------------------------------------------

#if wxUSE_DISPLAY
class wxDisplayFactoryGTK: public wxDisplayFactory
{
public:
    virtual wxDisplayImpl* CreateDisplay(unsigned n) wxOVERRIDE;
    virtual unsigned GetCount() wxOVERRIDE;
    virtual int GetFromPoint(const wxPoint& pt) wxOVERRIDE;
};

class wxDisplayImplGTK: public wxDisplayImpl
{
    typedef wxDisplayImpl base_type;
public:
    wxDisplayImplGTK(unsigned i);
    virtual wxRect GetGeometry() const wxOVERRIDE;
    virtual wxRect GetClientArea() const wxOVERRIDE;
    virtual wxString GetName() const wxOVERRIDE;
    virtual bool IsPrimary() const wxOVERRIDE;
    virtual wxArrayVideoModes GetModes(const wxVideoMode& mode) const wxOVERRIDE;
    virtual wxVideoMode GetCurrentMode() const wxOVERRIDE;
    virtual bool ChangeMode(const wxVideoMode& mode) wxOVERRIDE;

    GdkScreen* const m_screen;
};

static inline GdkScreen* GetScreen()
{
    return gdk_window_get_screen(wxGetTopLevelGDK());
}
//-----------------------------------------------------------------------------

wxDisplayImpl* wxDisplayFactoryGTK::CreateDisplay(unsigned n)
{
    return new wxDisplayImplGTK(n);
}

unsigned wxDisplayFactoryGTK::GetCount()
{
    return gdk_screen_get_n_monitors(GetScreen());
}

int wxDisplayFactoryGTK::GetFromPoint(const wxPoint& pt)
{
    GdkScreen* screen = GetScreen();
    int monitor = gdk_screen_get_monitor_at_point(screen, pt.x, pt.y);
    GdkRectangle rect;
    gdk_screen_get_monitor_geometry(screen, monitor, &rect);
    if (!wxRect(rect.x, rect.y, rect.width, rect.height).Contains(pt))
        monitor = wxNOT_FOUND;
    return monitor;
}
//-----------------------------------------------------------------------------

wxDisplayImplGTK::wxDisplayImplGTK(unsigned i)
    : base_type(i)
    , m_screen(GetScreen())
{
}

wxRect wxDisplayImplGTK::GetGeometry() const
{
    GdkRectangle rect;
    gdk_screen_get_monitor_geometry(m_screen, m_index, &rect);
    return wxRect(rect.x, rect.y, rect.width, rect.height);
}

wxRect wxDisplayImplGTK::GetClientArea() const
{
    GdkRectangle rect;
    gdk_screen_get_monitor_workarea(m_screen, m_index, &rect);
    return wxRect(rect.x, rect.y, rect.width, rect.height);
}

wxString wxDisplayImplGTK::GetName() const
{
    return wxString();
}

bool wxDisplayImplGTK::IsPrimary() const
{
    return gdk_screen_get_primary_monitor(m_screen) == int(m_index);
}

#ifdef GDK_WINDOWING_X11
wxArrayVideoModes wxXF86VidMode_GetModes(const wxVideoMode& mode, Display* pDisplay, int nScreen);
wxVideoMode wxXF86VidMode_GetCurrentMode(Display* display, int nScreen);
bool wxXF86VidMode_ChangeMode(const wxVideoMode& mode, Display* display, int nScreen);
wxArrayVideoModes wxX11_GetModes(const wxDisplayImpl* impl, const wxVideoMode& modeMatch, Display* display);
#endif

wxArrayVideoModes wxDisplayImplGTK::GetModes(const wxVideoMode& mode) const
{
    wxArrayVideoModes modes;
#ifdef GDK_WINDOWING_X11
#ifdef __WXGTK3__
    if (GDK_IS_X11_SCREEN(m_screen))
#endif
    {
        Display* display = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(m_screen));
#ifdef HAVE_X11_EXTENSIONS_XF86VMODE_H
        int nScreen = gdk_x11_screen_get_screen_number(m_screen);
        modes = wxXF86VidMode_GetModes(mode, display, nScreen);
#else
        modes = wxX11_GetModes(this, mode, display);
#endif
    }
#else
    wxUnusedVar(mode);
#endif
    return modes;
}

wxVideoMode wxDisplayImplGTK::GetCurrentMode() const
{
    wxVideoMode mode;
#if defined(GDK_WINDOWING_X11) && defined(HAVE_X11_EXTENSIONS_XF86VMODE_H)
#ifdef __WXGTK3__
    if (GDK_IS_X11_SCREEN(m_screen))
#endif
    {
        Display* display = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(m_screen));
        int nScreen = gdk_x11_screen_get_screen_number(m_screen);
        mode = wxXF86VidMode_GetCurrentMode(display, nScreen);
    }
#endif
    return mode;
}

bool wxDisplayImplGTK::ChangeMode(const wxVideoMode& mode)
{
    bool success = false;
#if defined(GDK_WINDOWING_X11) && defined(HAVE_X11_EXTENSIONS_XF86VMODE_H)
#ifdef __WXGTK3__
    if (GDK_IS_X11_SCREEN(m_screen))
#endif
    {
        Display* display = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(m_screen));
        int nScreen = gdk_x11_screen_get_screen_number(m_screen);
        success = wxXF86VidMode_ChangeMode(mode, display, nScreen);
    }
#else
    wxUnusedVar(mode);
#endif
    return success;
}

wxDisplayFactory* wxDisplay::CreateFactory()
{
    return new wxDisplayFactoryGTK;
}
#endif // wxUSE_DISPLAY
