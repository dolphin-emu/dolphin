/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/toplevel.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __VMS
#define XIconifyWindow XICONIFYWINDOW
#endif

#include "wx/toplevel.h"

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/icon.h"
    #include "wx/log.h"
#endif

#include "wx/evtloop.h"
#include "wx/sysopt.h"

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
    #include <gdk/gdkx.h>
    #include <X11/Xatom.h>  // XA_CARDINAL
    #include "wx/unix/utilsx11.h"
#endif
#ifdef GDK_WINDOWING_WAYLAND
    #include <gdk/gdkwayland.h>
    #define HAS_CLIENT_DECOR
#endif
#ifdef GDK_WINDOWING_MIR
    extern "C" {
    #include <gdk/gdkmir.h>
    }
    #define HAS_CLIENT_DECOR
#endif
#ifdef GDK_WINDOWING_BROADWAY
    #include <gdk/gdkbroadway.h>
    #define HAS_CLIENT_DECOR
#endif

#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"
#include "wx/gtk/private/win_gtk.h"

// ----------------------------------------------------------------------------
// data
// ----------------------------------------------------------------------------

// this is incremented while a modal dialog is shown
int wxOpenModalDialogsCount = 0;

// the frame that is currently active (i.e. its child has focus). It is
// used to generate wxActivateEvents
static wxTopLevelWindowGTK *g_activeFrame = NULL;

extern wxCursor g_globalCursor;
extern wxCursor g_busyCursor;

#ifdef GDK_WINDOWING_X11
// Whether _NET_REQUEST_FRAME_EXTENTS support is working
static enum {
    RFE_STATUS_UNKNOWN, RFE_STATUS_WORKING, RFE_STATUS_BROKEN
} gs_requestFrameExtentsStatus;

static bool gs_decorCacheValid;
#endif

#ifdef HAS_CLIENT_DECOR
static bool HasClientDecor(GtkWidget* widget)
{
    GdkDisplay* display = gtk_widget_get_display(widget);
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(display))
        return true;
#endif
#ifdef GDK_WINDOWING_MIR
    if (GDK_IS_MIR_DISPLAY(display))
        return true;
#endif
#ifdef GDK_WINDOWING_BROADWAY
    if (GDK_IS_BROADWAY_DISPLAY(display))
        return true;
#endif
    return false;
}
#endif // HAS_CLIENT_DECOR

//-----------------------------------------------------------------------------
// RequestUserAttention related functions
//-----------------------------------------------------------------------------

#ifndef __WXGTK3__
static void wxgtk_window_set_urgency_hint (GtkWindow *win,
                                           gboolean setting)
{
#if GTK_CHECK_VERSION(2,7,0)
    if (gtk_check_version(2,7,0) == NULL)
        gtk_window_set_urgency_hint(win, setting);
    else
#endif
    {
#ifdef GDK_WINDOWING_X11
        GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(win));
        wxCHECK_RET(window, "wxgtk_window_set_urgency_hint: GdkWindow not realized");

        Display* dpy = GDK_WINDOW_XDISPLAY(window);
        Window xid = GDK_WINDOW_XID(window);
        XWMHints* wm_hints = XGetWMHints(dpy, xid);

        if (!wm_hints)
            wm_hints = XAllocWMHints();

        if (setting)
            wm_hints->flags |= XUrgencyHint;
        else
            wm_hints->flags &= ~XUrgencyHint;

        XSetWMHints(dpy, xid, wm_hints);
        XFree(wm_hints);
#endif // GDK_WINDOWING_X11
    }
}
#define gtk_window_set_urgency_hint wxgtk_window_set_urgency_hint
#endif

extern "C" {
static gboolean gtk_frame_urgency_timer_callback( wxTopLevelWindowGTK *win )
{
    gtk_window_set_urgency_hint(GTK_WINDOW(win->m_widget), false);

    win->m_urgency_hint = -2;
    return FALSE;
}
}

//-----------------------------------------------------------------------------
// "focus_in_event"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean gtk_frame_focus_in_callback( GtkWidget *widget,
                                         GdkEvent *WXUNUSED(event),
                                         wxTopLevelWindowGTK *win )
{
    g_activeFrame = win;

    // MR: wxRequestUserAttention related block
    switch( win->m_urgency_hint )
    {
        default:
            g_source_remove( win->m_urgency_hint );
            // no break, fallthrough to remove hint too
        case -1:
            gtk_window_set_urgency_hint(GTK_WINDOW(widget), false);
            win->m_urgency_hint = -2;
            break;

        case -2: break;
    }

    wxActivateEvent event(wxEVT_ACTIVATE, true, g_activeFrame->GetId());
    event.SetEventObject(g_activeFrame);
    g_activeFrame->HandleWindowEvent(event);

    return FALSE;
}
}

//-----------------------------------------------------------------------------
// "focus_out_event"
//-----------------------------------------------------------------------------

extern "C" {
static
gboolean gtk_frame_focus_out_callback(GtkWidget * WXUNUSED(widget),
                                      GdkEventFocus *WXUNUSED(gdk_event),
                                      wxTopLevelWindowGTK * WXUNUSED(win))
{
    if (g_activeFrame)
    {
        wxActivateEvent event(wxEVT_ACTIVATE, false, g_activeFrame->GetId());
        event.SetEventObject(g_activeFrame);
        g_activeFrame->HandleWindowEvent(event);

        g_activeFrame = NULL;
    }

    return FALSE;
}
}

// ----------------------------------------------------------------------------
// "key_press_event"
// ----------------------------------------------------------------------------

extern "C" {
static gboolean
wxgtk_tlw_key_press_event(GtkWidget *widget, GdkEventKey *event)
{
    GtkWindow* const window = GTK_WINDOW(widget);

    // By default GTK+ checks for the menu accelerators in this (top level)
    // window first and then propagates the event to the currently focused
    // child from where it bubbles up the window parent chain. In wxWidgets,
    // however, we want the child window to have the event first but still
    // handle it as an accelerator if it's not processed there, so we need to
    // customize this by reversing the order of the steps done in the standard
    // GTK+ gtk_window_key_press_event() handler.

    if ( gtk_window_propagate_key_event(window, event) )
        return TRUE;

    if ( gtk_window_activate_key(window, event) )
        return TRUE;

    if (GTK_WIDGET_GET_CLASS(widget)->key_press_event(widget, event))
        return TRUE;

    return FALSE;
}
}

//-----------------------------------------------------------------------------
// "size_allocate" from m_wxwindow
//-----------------------------------------------------------------------------

extern "C" {
static void
size_allocate(GtkWidget*, GtkAllocation* alloc, wxTopLevelWindowGTK* win)
{
    win->m_useCachedClientSize = true;
    if (win->m_clientWidth  != alloc->width ||
        win->m_clientHeight != alloc->height)
    {
        win->m_clientWidth  = alloc->width;
        win->m_clientHeight = alloc->height;

        GtkAllocation a;
        gtk_widget_get_allocation(win->m_widget, &a);
        wxSize size(a.width, a.height);
#ifdef HAS_CLIENT_DECOR
        if (HasClientDecor(win->m_widget))
        {
            GtkAllocation a2;
            gtk_widget_get_allocation(win->m_mainWidget, &a2);
            wxTopLevelWindowGTK::DecorSize decorSize;
            decorSize.left = a2.x;
            decorSize.right = a.width - a2.width - a2.x;
            decorSize.top = a2.y;
            decorSize.bottom = a.height - a2.height - a2.y;
            win->GTKUpdateDecorSize(decorSize);
        }
        else
#endif
        {
            size.x += win->m_decorSize.left + win->m_decorSize.right;
            size.y += win->m_decorSize.top + win->m_decorSize.bottom;
        }
        win->m_width  = size.x;
        win->m_height = size.y;

        if (!win->IsIconized())
        {
            wxSizeEvent event(size, win->GetId());
            event.SetEventObject(win);
            win->HandleWindowEvent(event);
        }
        // else the window is currently unmapped, don't generate size events
    }
}
}

//-----------------------------------------------------------------------------
// "delete_event"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_frame_delete_callback( GtkWidget *WXUNUSED(widget),
                           GdkEvent *WXUNUSED(event),
                           wxTopLevelWindowGTK *win )
{
    if (win->IsEnabled() &&
        (wxOpenModalDialogsCount == 0 || (win->GetExtraStyle() & wxTOPLEVEL_EX_DIALOG) ||
         win->IsGrabbed()))
        win->Close();

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// "configure_event"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_frame_configure_callback( GtkWidget*,
                              GdkEventConfigure* gdk_event,
                              wxTopLevelWindowGTK *win )
{
    win->GTKConfigureEvent(gdk_event->x, gdk_event->y);
    return false;
}
}

void wxTopLevelWindowGTK::GTKConfigureEvent(int x, int y)
{
    wxPoint point;
#ifdef GDK_WINDOWING_X11
    if (gs_decorCacheValid)
    {
        const DecorSize& decorSize = GetCachedDecorSize();
        point.x = x - decorSize.left;
        point.y = y - decorSize.top;
    }
    else
#endif
    {
        gtk_window_get_position(GTK_WINDOW(m_widget), &point.x, &point.y);
    }

    if (m_x != point.x || m_y != point.y)
    {
        m_x = point.x;
        m_y = point.y;
        wxMoveEvent event(point, GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }
}

//-----------------------------------------------------------------------------
// "realize" from m_widget
//-----------------------------------------------------------------------------

// we cannot the WM hints and icons before the widget has been realized,
// so we do this directly after realization

void wxTopLevelWindowGTK::GTKHandleRealized()
{
    wxNonOwnedWindow::GTKHandleRealized();

    GdkWindow* window = gtk_widget_get_window(m_widget);

    gdk_window_set_decorations(window, (GdkWMDecoration)m_gdkDecor);
    gdk_window_set_functions(window, (GdkWMFunction)m_gdkFunc);

    const wxIconBundle& icons = GetIcons();
    if (icons.GetIconCount())
        SetIcons(icons);

    GdkCursor* cursor = g_globalCursor.GetCursor();
    if (wxIsBusy() && !gtk_window_get_modal(GTK_WINDOW(m_widget)))
        cursor = g_busyCursor.GetCursor();

    if (cursor)
        gdk_window_set_cursor(window, cursor);

#ifdef __WXGTK3__
    wxGCC_WARNING_SUPPRESS(deprecated-declarations)
    if (gtk_window_get_has_resize_grip(GTK_WINDOW(m_widget)))
    {
        // Grip window can end up obscured, probably due to deferred show.
        // Reset grip to ensure it is visible.
        gtk_window_set_has_resize_grip(GTK_WINDOW(m_widget), false);
        gtk_window_set_has_resize_grip(GTK_WINDOW(m_widget), true);
    }
    wxGCC_WARNING_RESTORE()
#endif
}

//-----------------------------------------------------------------------------
// "map_event" from m_widget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_frame_map_callback( GtkWidget*,
                        GdkEvent * WXUNUSED(event),
                        wxTopLevelWindow *win )
{
    const bool wasIconized = win->IsIconized();
    if (wasIconized)
    {
        // Because GetClientSize() returns (0,0) when IsIconized() is true,
        // a size event must be generated, just in case GetClientSize() was
        // called while iconized. This specifically happens when restoring a
        // tlw that was "rolled up" with some WMs.
        // Queue a resize rather than sending size event directly to allow
        // children to be made visible first.
        win->m_useCachedClientSize = false;
        win->m_clientWidth = 0;
        gtk_widget_queue_resize(win->m_wxwindow);
    }
    // it is possible for m_isShown to be false here, see bug #9909
    if (win->wxWindowBase::Show(true))
    {
        wxShowEvent eventShow(win->GetId(), true);
        eventShow.SetEventObject(win);
        win->GetEventHandler()->ProcessEvent(eventShow);
    }

    // restore focus-on-map setting in case ShowWithoutActivating() was called
    gtk_window_set_focus_on_map(GTK_WINDOW(win->m_widget), true);

    return false;
}
}

//-----------------------------------------------------------------------------
// "window-state-event" from m_widget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_frame_window_state_callback( GtkWidget* WXUNUSED(widget),
                          GdkEventWindowState *event,
                          wxTopLevelWindow *win )
{
    if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED)
        win->SetIconizeState((event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0);

    // if maximized bit changed and it is now set
    if (event->changed_mask & event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
    {
        wxMaximizeEvent evt(win->GetId());
        evt.SetEventObject(win);
        win->HandleWindowEvent(evt);
    }

    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
        win->m_fsIsShowing = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;

    return false;
}
}

//-----------------------------------------------------------------------------

bool wxGetFrameExtents(GdkWindow* window, int* left, int* right, int* top, int* bottom)
{
#ifdef GDK_WINDOWING_X11
    GdkDisplay* display = gdk_window_get_display(window);

    if (!GDK_IS_X11_DISPLAY(display))
        return false;

    static GdkAtom property = gdk_atom_intern("_NET_FRAME_EXTENTS", false);
    Atom xproperty = gdk_x11_atom_to_xatom_for_display(display, property);
    Atom type;
    int format;
    gulong nitems, bytes_after;
    guchar* data;
    Status status = XGetWindowProperty(
        GDK_DISPLAY_XDISPLAY(display),
        GDK_WINDOW_XID(window),
        xproperty,
        0, 4, false, XA_CARDINAL,
        &type, &format, &nitems, &bytes_after, &data);
    const bool success = status == Success && data && nitems == 4;
    if (success)
    {
        long* p = (long*)data;
        if (left)   *left   = int(p[0]);
        if (right)  *right  = int(p[1]);
        if (top)    *top    = int(p[2]);
        if (bottom) *bottom = int(p[3]);
    }
    if (data)
        XFree(data);
    return success;
#else
    return false;
#endif
}

#ifdef GDK_WINDOWING_X11
//-----------------------------------------------------------------------------
// "property_notify_event" from m_widget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean property_notify_event(
    GtkWidget*, GdkEventProperty* event, wxTopLevelWindowGTK* win)
{
    // Watch for changes to _NET_FRAME_EXTENTS property
    static GdkAtom property = gdk_atom_intern("_NET_FRAME_EXTENTS", false);
    if (event->state == GDK_PROPERTY_NEW_VALUE && event->atom == property)
    {
        if (win->m_netFrameExtentsTimerId)
        {
            // WM support for _NET_REQUEST_FRAME_EXTENTS is working
            gs_requestFrameExtentsStatus = RFE_STATUS_WORKING;
            g_source_remove(win->m_netFrameExtentsTimerId);
            win->m_netFrameExtentsTimerId = 0;
        }

        wxTopLevelWindowGTK::DecorSize decorSize = win->m_decorSize;
        gs_decorCacheValid = wxGetFrameExtents(event->window,
            &decorSize.left, &decorSize.right, &decorSize.top, &decorSize.bottom);

        win->GTKUpdateDecorSize(decorSize);
    }
    return false;
}
}

extern "C" {
static gboolean request_frame_extents_timeout(void* data)
{
    // WM support for _NET_REQUEST_FRAME_EXTENTS is broken
    gs_requestFrameExtentsStatus = RFE_STATUS_BROKEN;
    gdk_threads_enter();
    wxTopLevelWindowGTK* win = static_cast<wxTopLevelWindowGTK*>(data);
    win->m_netFrameExtentsTimerId = 0;
    wxTopLevelWindowGTK::DecorSize decorSize = win->m_decorSize;
    wxGetFrameExtents(gtk_widget_get_window(win->m_widget),
        &decorSize.left, &decorSize.right, &decorSize.top, &decorSize.bottom);
    win->GTKUpdateDecorSize(decorSize);
    gdk_threads_leave();
    return false;
}
}
#endif // GDK_WINDOWING_X11

// ----------------------------------------------------------------------------
// wxTopLevelWindowGTK creation
// ----------------------------------------------------------------------------

void wxTopLevelWindowGTK::Init()
{
    m_mainWidget = NULL;
    m_isIconized = false;
    m_fsIsShowing = false;
    m_themeEnabled = true;
    m_gdkDecor =
    m_gdkFunc = 0;
    m_grabbed = false;
    m_deferShow = true;
    m_deferShowAllowed = true;
    m_updateDecorSize = true;
    m_netFrameExtentsTimerId = 0;
    m_incWidth = m_incHeight = 0;
    memset(&m_decorSize, 0, sizeof(m_decorSize));

    m_urgency_hint = -2;
}

bool wxTopLevelWindowGTK::Create( wxWindow *parent,
                                  wxWindowID id,
                                  const wxString& title,
                                  const wxPoint& pos,
                                  const wxSize& sizeOrig,
                                  long style,
                                  const wxString &name )
{
    wxSize size(sizeOrig);
    if (!size.IsFullySpecified())
        size.SetDefaults(GetDefaultSize());

    wxTopLevelWindows.Append( this );

    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
    {
        wxFAIL_MSG( wxT("wxTopLevelWindowGTK creation failed") );
        return false;
    }

    m_title = title;

    // NB: m_widget may be !=NULL if it was created by derived class' Create,
    //     e.g. in wxTaskBarIconAreaGTK
    if (m_widget == NULL)
    {
        m_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        if (GetExtraStyle() & wxTOPLEVEL_EX_DIALOG)
        {
            // Tell WM that this is a dialog window and make it center
            // on parent by default (this is what GtkDialog ctor does):
            gtk_window_set_type_hint(GTK_WINDOW(m_widget),
                                     GDK_WINDOW_TYPE_HINT_DIALOG);
            gtk_window_set_position(GTK_WINDOW(m_widget),
                                    GTK_WIN_POS_CENTER_ON_PARENT);
        }
        else
        {
            if (style & wxFRAME_TOOL_WINDOW)
            {
                gtk_window_set_type_hint(GTK_WINDOW(m_widget),
                                         GDK_WINDOW_TYPE_HINT_UTILITY);

                // On some WMs, like KDE, a TOOL_WINDOW will still show
                // on the taskbar, but on Gnome a TOOL_WINDOW will not.
                // For consistency between WMs and with Windows, we
                // should set the NO_TASKBAR flag which will apply
                // the set_skip_taskbar_hint if it is available,
                // ensuring no taskbar entry will appear.
                style |= wxFRAME_NO_TASKBAR;
            }
        }

        g_object_ref(m_widget);
    }

    wxWindow *topParent = wxGetTopLevelParent(m_parent);
    if (topParent && (((GTK_IS_WINDOW(topParent->m_widget)) &&
                       (GetExtraStyle() & wxTOPLEVEL_EX_DIALOG)) ||
                       (style & wxFRAME_FLOAT_ON_PARENT)))
    {
        gtk_window_set_transient_for( GTK_WINDOW(m_widget),
                                      GTK_WINDOW(topParent->m_widget) );
    }

    if (style & wxFRAME_NO_TASKBAR)
    {
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(m_widget), TRUE);
    }

    if (style & wxSTAY_ON_TOP)
    {
        gtk_window_set_keep_above(GTK_WINDOW(m_widget), TRUE);
    }
    if (style & wxMAXIMIZE)
        gtk_window_maximize(GTK_WINDOW(m_widget));

#if 0
    if (!name.empty())
        gtk_window_set_role( GTK_WINDOW(m_widget), wxGTK_CONV( name ) );
#endif

    gtk_window_set_title( GTK_WINDOW(m_widget), wxGTK_CONV( title ) );
    gtk_widget_set_can_focus(m_widget, false);

    g_signal_connect (m_widget, "delete_event",
                      G_CALLBACK (gtk_frame_delete_callback), this);

    // m_mainWidget is a GtkVBox, holding the bars and client area (m_wxwindow)
    m_mainWidget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_show( m_mainWidget );
    gtk_widget_set_can_focus(m_mainWidget, false);
    gtk_container_add( GTK_CONTAINER(m_widget), m_mainWidget );

    // m_wxwindow is the client area
    m_wxwindow = wxPizza::New();
    gtk_widget_show( m_wxwindow );
    gtk_box_pack_start(GTK_BOX(m_mainWidget), m_wxwindow, true, true, 0);

    // we donm't allow the frame to get the focus as otherwise
    // the frame will grab it at arbitrary focus changes
    gtk_widget_set_can_focus(m_wxwindow, false);

    if (m_parent) m_parent->AddChild( this );

    g_signal_connect(m_wxwindow, "size_allocate",
        G_CALLBACK(size_allocate), this);

    PostCreation();

#ifndef __WXGTK3__
    if (pos != wxDefaultPosition)
        gtk_widget_set_uposition( m_widget, m_x, m_y );
#endif

    // for some reported size corrections
    g_signal_connect (m_widget, "map_event",
                      G_CALLBACK (gtk_frame_map_callback), this);

    // for iconized state
    g_signal_connect (m_widget, "window_state_event",
                      G_CALLBACK (gtk_frame_window_state_callback), this);


    // for wxMoveEvent
    g_signal_connect (m_widget, "configure_event",
                      G_CALLBACK (gtk_frame_configure_callback), this);

    // activation
    g_signal_connect_after (m_widget, "focus_in_event",
                      G_CALLBACK (gtk_frame_focus_in_callback), this);
    g_signal_connect_after (m_widget, "focus_out_event",
                      G_CALLBACK (gtk_frame_focus_out_callback), this);

    // We need to customize the default GTK+ logic for key processing to make
    // it conforming to wxWidgets event processing order.
    g_signal_connect (m_widget, "key_press_event",
                      G_CALLBACK (wxgtk_tlw_key_press_event), NULL);

#ifdef GDK_WINDOWING_X11
#ifdef __WXGTK3__
    if (GDK_IS_X11_SCREEN(gtk_window_get_screen(GTK_WINDOW(m_widget))))
#endif
    {
        gtk_widget_add_events(m_widget, GDK_PROPERTY_CHANGE_MASK);
        g_signal_connect(m_widget, "property_notify_event",
            G_CALLBACK(property_notify_event), this);
    }
#endif // GDK_WINDOWING_X11

    // translate wx decorations styles into Motif WM hints (they are recognized
    // by other WMs as well)

    // always enable moving the window as we have no separate flag for enabling
    // it
    m_gdkFunc = GDK_FUNC_MOVE;

    if ( style & wxCLOSE_BOX )
        m_gdkFunc |= GDK_FUNC_CLOSE;

    if ( style & wxMINIMIZE_BOX )
        m_gdkFunc |= GDK_FUNC_MINIMIZE;

    if ( style & wxMAXIMIZE_BOX )
        m_gdkFunc |= GDK_FUNC_MAXIMIZE;

    if ( (style & wxSIMPLE_BORDER) || (style & wxNO_BORDER) )
    {
        m_gdkDecor = 0;
        gtk_window_set_decorated(GTK_WINDOW(m_widget), false);
    }
    else // have border
    {
        m_gdkDecor = GDK_DECOR_BORDER;

        if ( style & wxCAPTION )
            m_gdkDecor |= GDK_DECOR_TITLE;
#if defined(GDK_WINDOWING_WAYLAND) && GTK_CHECK_VERSION(3,10,0)
        else if (
            GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(m_widget)) &&
            gtk_check_version(3,10,0) == NULL)
        {
            gtk_window_set_titlebar(GTK_WINDOW(m_widget), gtk_header_bar_new());
        }
#endif

        if ( style & wxSYSTEM_MENU )
            m_gdkDecor |= GDK_DECOR_MENU;

        if ( style & wxMINIMIZE_BOX )
            m_gdkDecor |= GDK_DECOR_MINIMIZE;

        if ( style & wxMAXIMIZE_BOX )
            m_gdkDecor |= GDK_DECOR_MAXIMIZE;

        if ( style & wxRESIZE_BORDER )
        {
           m_gdkFunc |= GDK_FUNC_RESIZE;
           m_gdkDecor |= GDK_DECOR_RESIZEH;
        }
    }

    m_decorSize = GetCachedDecorSize();
    int w, h;
    GTKDoGetSize(&w, &h);

    if (style & wxRESIZE_BORDER)
    {
        gtk_window_set_default_size(GTK_WINDOW(m_widget), w, h);
#ifndef __WXGTK3__
        gtk_window_set_policy(GTK_WINDOW(m_widget), 1, 1, 1);
#endif
    }
    else
    {
        gtk_window_set_resizable(GTK_WINDOW(m_widget), false);
        // gtk_window_set_default_size() does not work for un-resizable windows,
        // unless you set the size hints, but that causes Ubuntu's WM to make
        // the window resizable even though GDK_FUNC_RESIZE is not set.
        gtk_widget_set_size_request(m_widget, w, h);
    }

    return true;
}

wxTopLevelWindowGTK::~wxTopLevelWindowGTK()
{
    if ( m_netFrameExtentsTimerId )
    {
        // Don't let the timer callback fire as the window pointer passed to it
        // will become invalid very soon.
        g_source_remove(m_netFrameExtentsTimerId);
    }

    if (m_grabbed)
    {
        wxFAIL_MSG(wxT("Window still grabbed"));
        RemoveGrab();
    }

    SendDestroyEvent();

    // it may also be GtkScrolledWindow in the case of an MDI child
    if (GTK_IS_WINDOW(m_widget))
    {
        gtk_window_set_focus( GTK_WINDOW(m_widget), NULL );
    }

    if (g_activeFrame == this)
        g_activeFrame = NULL;
}

bool wxTopLevelWindowGTK::EnableCloseButton( bool enable )
{
    if (enable)
        m_gdkFunc |= GDK_FUNC_CLOSE;
    else
        m_gdkFunc &= ~GDK_FUNC_CLOSE;

    GdkWindow* window = gtk_widget_get_window(m_widget);
    if (window)
        gdk_window_set_functions(window, (GdkWMFunction)m_gdkFunc);

    return true;
}

bool wxTopLevelWindowGTK::ShowFullScreen(bool show, long)
{
    if (show == m_fsIsShowing)
        return false; // return what?

    m_fsIsShowing = show;

#ifdef GDK_WINDOWING_X11
    GdkScreen* screen = gtk_widget_get_screen(m_widget);
    GdkDisplay* display = gdk_screen_get_display(screen);
    Display* xdpy = NULL;
    Window xroot = None;
    wxX11FullScreenMethod method = wxX11_FS_WMSPEC;

    if (GDK_IS_X11_DISPLAY(display))
    {
        xdpy = GDK_DISPLAY_XDISPLAY(display);
        xroot = GDK_WINDOW_XID(gdk_screen_get_root_window(screen));
        method = wxGetFullScreenMethodX11(xdpy, (WXWindow)xroot);
    }

    // NB: gtk_window_fullscreen() uses freedesktop.org's WMspec extensions
    //     to switch to fullscreen, which is not always available. We must
    //     check if WM supports the spec and use legacy methods if it
    //     doesn't.
    if ( method == wxX11_FS_WMSPEC )
#endif // GDK_WINDOWING_X11
    {
        if (show)
            gtk_window_fullscreen( GTK_WINDOW( m_widget ) );
        else
            gtk_window_unfullscreen( GTK_WINDOW( m_widget ) );
    }
#ifdef GDK_WINDOWING_X11
    else if (xdpy != NULL)
    {
        GdkWindow* window = gtk_widget_get_window(m_widget);
        Window xid = GDK_WINDOW_XID(window);

        if (show)
        {
            GetPosition( &m_fsSaveFrame.x, &m_fsSaveFrame.y );
            GetSize( &m_fsSaveFrame.width, &m_fsSaveFrame.height );

            const int screen_width = gdk_screen_get_width(screen);
            const int screen_height = gdk_screen_get_height(screen);

            gint client_x, client_y, root_x, root_y;
            gint width, height;

            m_fsSaveGdkFunc = m_gdkFunc;
            m_fsSaveGdkDecor = m_gdkDecor;
            m_gdkFunc = m_gdkDecor = 0;
            gdk_window_set_decorations(window, (GdkWMDecoration)0);
            gdk_window_set_functions(window, (GdkWMFunction)0);

            gdk_window_get_origin(window, &root_x, &root_y);
            gdk_window_get_geometry(window, &client_x, &client_y, &width, &height);

            gdk_window_move_resize(
                window, -client_x, -client_y, screen_width + 1, screen_height + 1);

            wxSetFullScreenStateX11(xdpy,
                                    (WXWindow)xroot,
                                    (WXWindow)xid,
                                    show, &m_fsSaveFrame, method);
        }
        else // hide
        {
            m_gdkFunc = m_fsSaveGdkFunc;
            m_gdkDecor = m_fsSaveGdkDecor;
            gdk_window_set_decorations(window, (GdkWMDecoration)m_gdkDecor);
            gdk_window_set_functions(window, (GdkWMFunction)m_gdkFunc);

            wxSetFullScreenStateX11(xdpy,
                                    (WXWindow)xroot,
                                    (WXWindow)xid,
                                    show, &m_fsSaveFrame, method);

            SetSize(m_fsSaveFrame.x, m_fsSaveFrame.y,
                    m_fsSaveFrame.width, m_fsSaveFrame.height);
        }
    }
#endif // GDK_WINDOWING_X11

    // documented behaviour is to show the window if it's still hidden when
    // showing it full screen
    if (show)
        Show();

    return true;
}

// ----------------------------------------------------------------------------
// overridden wxWindow methods
// ----------------------------------------------------------------------------

void wxTopLevelWindowGTK::Refresh( bool WXUNUSED(eraseBackground), const wxRect *WXUNUSED(rect) )
{
    wxCHECK_RET( m_widget, wxT("invalid frame") );

    gtk_widget_queue_draw( m_widget );

    GdkWindow* window = NULL;
    if (m_wxwindow)
        window = gtk_widget_get_window(m_wxwindow);
    if (window)
        gdk_window_invalidate_rect(window, NULL, true);
}

bool wxTopLevelWindowGTK::Show( bool show )
{
    wxCHECK_MSG(m_widget, false, "invalid frame");

#ifdef GDK_WINDOWING_X11
    bool deferShow = show && !m_isShown && m_deferShow;
    if (deferShow)
    {
        deferShow = m_deferShowAllowed &&
            gs_requestFrameExtentsStatus != RFE_STATUS_BROKEN &&
            !gtk_widget_get_realized(m_widget) &&
            GDK_IS_X11_DISPLAY(gtk_widget_get_display(m_widget)) &&
            g_signal_handler_find(m_widget,
                GSignalMatchType(G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
                g_signal_lookup("property_notify_event", GTK_TYPE_WIDGET),
                0, NULL, NULL, this);
        if (deferShow)
        {
            GdkScreen* screen = gtk_widget_get_screen(m_widget);
            GdkAtom atom = gdk_atom_intern("_NET_REQUEST_FRAME_EXTENTS", false);
            deferShow = gdk_x11_screen_supports_net_wm_hint(screen, atom) != 0;

            // If _NET_REQUEST_FRAME_EXTENTS not supported, don't allow changes
            // to m_decorSize, it breaks saving/restoring window size with
            // GetSize()/SetSize() because it makes window bigger between each
            // restore and save.
            m_updateDecorSize = deferShow;
        }

        m_deferShow = deferShow;
    }
    if (deferShow)
    {
        // Initial show. If WM supports _NET_REQUEST_FRAME_EXTENTS, defer
        // calling gtk_widget_show() until _NET_FRAME_EXTENTS property
        // notification is received, so correct frame extents are known.
        // This allows resizing m_widget to keep the overall size in sync with
        // what wxWidgets expects it to be without an obvious change in the
        // window size immediately after it becomes visible.

        // Realize m_widget, so m_widget->window can be used. Realizing normally
        // causes the widget tree to be size_allocated, which generates size
        // events in the wrong order. However, the size_allocates will not be
        // done if the allocation is not the default (1,1).
        GtkAllocation alloc;
        gtk_widget_get_allocation(m_widget, &alloc);
        const int alloc_width = alloc.width;
        if (alloc_width == 1)
        {
            alloc.width = 2;
            gtk_widget_set_allocation(m_widget, &alloc);
        }
        gtk_widget_realize(m_widget);
        if (alloc_width == 1)
        {
            alloc.width = 1;
            gtk_widget_set_allocation(m_widget, &alloc);
        }

        // send _NET_REQUEST_FRAME_EXTENTS
        XClientMessageEvent xevent;
        memset(&xevent, 0, sizeof(xevent));
        xevent.type = ClientMessage;
        GdkWindow* window = gtk_widget_get_window(m_widget);
        xevent.window = GDK_WINDOW_XID(window);
        xevent.message_type = gdk_x11_atom_to_xatom_for_display(
            gdk_window_get_display(window),
            gdk_atom_intern("_NET_REQUEST_FRAME_EXTENTS", false));
        xevent.format = 32;
        Display* display = GDK_DISPLAY_XDISPLAY(gdk_window_get_display(window));
        XSendEvent(display, DefaultRootWindow(display), false,
            SubstructureNotifyMask | SubstructureRedirectMask,
            (XEvent*)&xevent);

        if (gs_requestFrameExtentsStatus == RFE_STATUS_UNKNOWN)
        {
            // if WM does not respond to request within 1 second,
            // we assume support for _NET_REQUEST_FRAME_EXTENTS is not working
            m_netFrameExtentsTimerId =
                g_timeout_add(1000, request_frame_extents_timeout, this);
        }

        // defer calling gtk_widget_show()
        m_isShown = true;
        return true;
    }
#endif // GDK_WINDOWING_X11

    if (show && !gtk_widget_get_realized(m_widget))
    {
        // size_allocate signals occur in reverse order (bottom to top).
        // Things work better if the initial wxSizeEvents are sent (from the
        // top down), before the initial size_allocate signals occur.
        wxSizeEvent event(GetSize(), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);

#ifdef __WXGTK3__
        GTKSizeRevalidate();
#endif
    }

    bool change = base_type::Show(show);

    if (change && !show)
    {
        // make sure window has a non-default position, so when it is shown
        // again, it won't be repositioned by WM as if it were a new window
        // Note that this must be done _after_ the window is hidden.
        gtk_window_move((GtkWindow*)m_widget, m_x, m_y);
    }

    return change;
}

void wxTopLevelWindowGTK::ShowWithoutActivating()
{
    if (!m_isShown)
    {
        gtk_window_set_focus_on_map(GTK_WINDOW(m_widget), false);
        Show(true);
    }
}

void wxTopLevelWindowGTK::Raise()
{
    gtk_window_present( GTK_WINDOW( m_widget ) );
}

void wxTopLevelWindowGTK::DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y), int WXUNUSED(width), int WXUNUSED(height) )
{
    wxFAIL_MSG( wxT("DoMoveWindow called for wxTopLevelWindowGTK") );
}

// ----------------------------------------------------------------------------
// window geometry
// ----------------------------------------------------------------------------

void wxTopLevelWindowGTK::GTKDoGetSize(int *width, int *height) const
{
    wxSize size(m_width, m_height);
#ifdef HAS_CLIENT_DECOR
    if (!HasClientDecor(m_widget))
#endif
    {
        size.x -= m_decorSize.left + m_decorSize.right;
        size.y -= m_decorSize.top + m_decorSize.bottom;
    }
    if (size.x < 0) size.x = 0;
    if (size.y < 0) size.y = 0;
    if (width)  *width  = size.x;
    if (height) *height = size.y;
}

void wxTopLevelWindowGTK::DoSetSize( int x, int y, int width, int height, int sizeFlags )
{
    wxCHECK_RET( m_widget, wxT("invalid frame") );

    // deal with the position first
    int old_x = m_x;
    int old_y = m_y;

    if ( !(sizeFlags & wxSIZE_ALLOW_MINUS_ONE) )
    {
        // -1 means "use existing" unless the flag above is specified
        if ( x != -1 )
            m_x = x;
        if ( y != -1 )
            m_y = y;
    }
    else // wxSIZE_ALLOW_MINUS_ONE
    {
        m_x = x;
        m_y = y;
    }

    const wxSize oldSize(m_width, m_height);
    if (width >= 0)
        m_width = width;
    if (height >= 0)
        m_height = height;
    ConstrainSize();
    if (m_width < 1) m_width = 1;
    if (m_height < 1) m_height = 1;

    if ( m_x != old_x || m_y != old_y )
    {
        gtk_window_move( GTK_WINDOW(m_widget), m_x, m_y );
        wxMoveEvent event(wxPoint(m_x, m_y), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }

    if (m_width != oldSize.x || m_height != oldSize.y)
    {
        m_deferShowAllowed = true;
        m_useCachedClientSize = false;

        int w, h;
        GTKDoGetSize(&w, &h);
        gtk_window_resize(GTK_WINDOW(m_widget), w, h);
        if (!gtk_window_get_resizable(GTK_WINDOW(m_widget)))
            gtk_widget_set_size_request(GTK_WIDGET(m_widget), w, h);

        DoGetClientSize(&m_clientWidth, &m_clientHeight);
        wxSizeEvent event(GetSize(), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }
}

extern "C" {
static gboolean reset_size_request(void* data)
{
    gtk_widget_set_size_request(GTK_WIDGET(data), -1, -1);
    return false;
}
}

void wxTopLevelWindowGTK::DoSetClientSize(int width, int height)
{
    base_type::DoSetClientSize(width, height);

    // Since client size is being explicitly set, don't change it later
    // Has to be done after calling base because it calls SetSize,
    // which sets this true
    m_deferShowAllowed = false;

    if (m_wxwindow)
    {
        // If window is not resizable or not yet shown, set size request on
        // client widget, to make it more likely window will get correct size
        // even if our decorations size cache is incorrect (as it will be before
        // showing first TLW).
        if (!gtk_window_get_resizable(GTK_WINDOW(m_widget)))
        {
            gtk_widget_set_size_request(m_widget, -1, -1);
            gtk_widget_set_size_request(m_wxwindow, m_clientWidth, m_clientHeight);
        }
        else if (!IsShown())
        {
            gtk_widget_set_size_request(m_wxwindow, m_clientWidth, m_clientHeight);
            // Cancel size request at next idle to allow resizing
            g_idle_add_full(G_PRIORITY_LOW, reset_size_request, m_wxwindow, NULL);
        }
    }
}

void wxTopLevelWindowGTK::DoGetClientSize( int *width, int *height ) const
{
    wxCHECK_RET(m_widget, "invalid frame");

    if ( IsIconized() )
    {
        // for consistency with wxMSW, client area is supposed to be empty for
        // the iconized windows
        if ( width )
            *width = 0;
        if ( height )
            *height = 0;
    }
    else if (m_useCachedClientSize)
        base_type::DoGetClientSize(width, height);
    else
    {
        int w = m_width - (m_decorSize.left + m_decorSize.right);
        int h = m_height - (m_decorSize.top + m_decorSize.bottom);
        if (w < 0) w = 0;
        if (h < 0) h = 0;
        if (width) *width = w;
        if (height) *height = h;
    }
}

void wxTopLevelWindowGTK::DoSetSizeHints( int minW, int minH,
                                          int maxW, int maxH,
                                          int incW, int incH )
{
    base_type::DoSetSizeHints(minW, minH, maxW, maxH, incW, incH);
    m_incWidth = incW;
    m_incHeight = incH;

    const wxSize minSize = GetMinSize();
    const wxSize maxSize = GetMaxSize();
    GdkGeometry hints;
    // always set both min and max hints, otherwise GTK will
    // make assumptions we don't want about the unset values
    int hints_mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
    hints.min_width = 1;
    hints.min_height = 1;
    hints.max_width = INT_MAX;
    hints.max_height = INT_MAX;
    int decorSize_x;
    int decorSize_y;
#ifdef HAS_CLIENT_DECOR
    if (HasClientDecor(m_widget))
    {
        decorSize_x = 0;
        decorSize_y = 0;
    }
    else
#endif
    {
        decorSize_x = m_decorSize.left + m_decorSize.right;
        decorSize_y = m_decorSize.top + m_decorSize.bottom;
    }
    if (minSize.x > decorSize_x)
        hints.min_width = minSize.x - decorSize_x;
    if (minSize.y > decorSize_y)
        hints.min_height = minSize.y - decorSize_y;
    if (maxSize.x > 0)
    {
        hints.max_width = maxSize.x - decorSize_x;
        if (hints.max_width < hints.min_width)
            hints.max_width = hints.min_width;
    }
    if (maxSize.y > 0)
    {
        hints.max_height = maxSize.y - decorSize_y;
        if (hints.max_height < hints.min_height)
            hints.max_height = hints.min_height;
    }
    if (incW > 0 || incH > 0)
    {
        hints_mask |= GDK_HINT_RESIZE_INC;
        hints.width_inc  = incW > 0 ? incW : 1;
        hints.height_inc = incH > 0 ? incH : 1;
    }
    gtk_window_set_geometry_hints(
        (GtkWindow*)m_widget, NULL, &hints, (GdkWindowHints)hints_mask);
}

void wxTopLevelWindowGTK::GTKUpdateDecorSize(const DecorSize& decorSize)
{
    if (!IsMaximized() && !IsFullScreen())
        GetCachedDecorSize() = decorSize;

#ifdef HAS_CLIENT_DECOR
    if (HasClientDecor(m_widget))
    {
        m_decorSize = decorSize;
        return;
    }
#endif
#ifdef GDK_WINDOWING_X11
    if (m_updateDecorSize && memcmp(&m_decorSize, &decorSize, sizeof(DecorSize)))
    {
        m_useCachedClientSize = false;
        const wxSize diff(
            decorSize.left - m_decorSize.left + decorSize.right - m_decorSize.right,
            decorSize.top - m_decorSize.top + decorSize.bottom - m_decorSize.bottom);
        m_decorSize = decorSize;
        bool resized = false;
        if (m_minWidth > 0 || m_minHeight > 0 || m_maxWidth > 0 || m_maxHeight > 0)
        {
            // update size hints, they depend on m_decorSize
            if (!m_deferShow)
            {
                // if size hints match old size, assume hints were set to
                // maintain current client size, and adjust hints accordingly
                if (m_minWidth == m_height) m_minWidth += diff.x;
                if (m_maxWidth == m_height) m_maxWidth += diff.x;
                if (m_minHeight == m_height) m_minHeight += diff.y;
                if (m_maxHeight == m_height) m_maxHeight += diff.y;
            }
            DoSetSizeHints(m_minWidth, m_minHeight, m_maxWidth, m_maxHeight, m_incWidth, m_incHeight);
        }
        if (m_deferShow)
        {
            // keep overall size unchanged by shrinking m_widget
            int w, h;
            GTKDoGetSize(&w, &h);
            // but not if size would be less than minimum, it won't take effect
            if (w >= m_minWidth - (decorSize.left + decorSize.right) &&
                h >= m_minHeight - (decorSize.top + decorSize.bottom))
            {
                gtk_window_resize(GTK_WINDOW(m_widget), w, h);
                if (!gtk_window_get_resizable(GTK_WINDOW(m_widget)))
                    gtk_widget_set_size_request(GTK_WIDGET(m_widget), w, h);
                resized = true;
            }
        }
        if (!resized)
        {
            // adjust overall size to match change in frame extents
            m_width  += diff.x;
            m_height += diff.y;
            if (m_width  < 1) m_width  = 1;
            if (m_height < 1) m_height = 1;
            m_clientWidth = 0;
            gtk_widget_queue_resize(m_wxwindow);
        }
    }
    if (m_deferShow)
    {
        // gtk_widget_show() was deferred, do it now
        m_deferShow = false;
        DoGetClientSize(&m_clientWidth, &m_clientHeight);
        wxSizeEvent sizeEvent(GetSize(), GetId());
        sizeEvent.SetEventObject(this);
        HandleWindowEvent(sizeEvent);

#ifdef __WXGTK3__
        GTKSizeRevalidate();
#endif
        gtk_widget_show(m_widget);

        wxShowEvent showEvent(GetId(), true);
        showEvent.SetEventObject(this);
        HandleWindowEvent(showEvent);
    }
#endif // GDK_WINDOWING_X11
}

wxTopLevelWindowGTK::DecorSize& wxTopLevelWindowGTK::GetCachedDecorSize()
{
    static DecorSize size[8];

    int index = 0;
    // title bar
    if (m_gdkDecor & (GDK_DECOR_MENU | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE | GDK_DECOR_TITLE))
        index = 1;
    // border
    if (m_gdkDecor & GDK_DECOR_BORDER)
        index |= 2;
    // utility window decor can be different
    if (m_windowStyle & wxFRAME_TOOL_WINDOW)
        index |= 4;
    return size[index];
}

// ----------------------------------------------------------------------------
// frame title/icon
// ----------------------------------------------------------------------------

void wxTopLevelWindowGTK::SetTitle( const wxString &title )
{
    wxCHECK_RET(m_widget, "invalid frame");

    if ( title == m_title )
        return;

    m_title = title;

    gtk_window_set_title( GTK_WINDOW(m_widget), wxGTK_CONV( title ) );
}

void wxTopLevelWindowGTK::SetIcons( const wxIconBundle &icons )
{
    base_type::SetIcons(icons);

    // Setting icons before window is realized can cause a GTK assertion if
    // another TLW is realized before this one, and it has this one as its
    // transient parent. The life demo exibits this problem.
    if (m_widget && gtk_widget_get_realized(m_widget))
    {
        GList* list = NULL;
        for (size_t i = icons.GetIconCount(); i--;)
            list = g_list_prepend(list, icons.GetIconByIndex(i).GetPixbuf());
        gtk_window_set_icon_list(GTK_WINDOW(m_widget), list);
        g_list_free(list);
    }
}

// ----------------------------------------------------------------------------
// frame state: maximized/iconized/normal
// ----------------------------------------------------------------------------

void wxTopLevelWindowGTK::Maximize(bool maximize)
{
    if (maximize)
        gtk_window_maximize( GTK_WINDOW( m_widget ) );
    else
        gtk_window_unmaximize( GTK_WINDOW( m_widget ) );
}

bool wxTopLevelWindowGTK::IsMaximized() const
{
    GdkWindow* window = NULL;
    if (m_widget)
        window = gtk_widget_get_window(m_widget);
    return window && (gdk_window_get_state(window) & GDK_WINDOW_STATE_MAXIMIZED);
}

void wxTopLevelWindowGTK::Restore()
{
    // "Present" seems similar enough to "restore"
    gtk_window_present( GTK_WINDOW( m_widget ) );
}

void wxTopLevelWindowGTK::Iconize( bool iconize )
{
    if (iconize)
        gtk_window_iconify( GTK_WINDOW( m_widget ) );
    else
        gtk_window_deiconify( GTK_WINDOW( m_widget ) );
}

bool wxTopLevelWindowGTK::IsIconized() const
{
    return m_isIconized;
}

void wxTopLevelWindowGTK::SetIconizeState(bool iconize)
{
    if ( iconize != m_isIconized )
    {
        m_isIconized = iconize;
        (void)SendIconizeEvent(iconize);
    }
}

void wxTopLevelWindowGTK::AddGrab()
{
    if (!m_grabbed)
    {
        m_grabbed = true;
        gtk_grab_add( m_widget );
        wxGUIEventLoop().Run();
        gtk_grab_remove( m_widget );
    }
}

void wxTopLevelWindowGTK::RemoveGrab()
{
    if (m_grabbed)
    {
        gtk_main_quit();
        m_grabbed = false;
    }
}

bool wxTopLevelWindowGTK::IsActive()
{
    return (this == (wxTopLevelWindowGTK*)g_activeFrame);
}

void wxTopLevelWindowGTK::RequestUserAttention(int flags)
{
    bool new_hint_value = false;

    // FIXME: This is a workaround to focus handling problem
    // If RequestUserAttention is called for example right after a wxSleep, OnInternalIdle
    // hasn't yet been processed, and the internal focus system is not up to date yet.
    // YieldFor(wxEVT_CATEGORY_UI) ensures the processing of it (hopefully it
    // won't have side effects) - MR
    wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI);

    if(m_urgency_hint >= 0)
        g_source_remove(m_urgency_hint);

    m_urgency_hint = -2;

    if( gtk_widget_get_realized(m_widget) && !IsActive() )
    {
        new_hint_value = true;

        if (flags & wxUSER_ATTENTION_INFO)
        {
            m_urgency_hint = g_timeout_add(5000, (GSourceFunc)gtk_frame_urgency_timer_callback, this);
        } else {
            m_urgency_hint = -1;
        }
    }

    gtk_window_set_urgency_hint(GTK_WINDOW(m_widget), new_hint_value);
}

void wxTopLevelWindowGTK::SetWindowStyleFlag( long style )
{
    // Store which styles were changed
    long styleChanges = style ^ m_windowStyle;

    // Process wxWindow styles. This also updates the internal variable
    // Therefore m_windowStyle bits carry now the _new_ style values
    wxWindow::SetWindowStyleFlag(style);

    // just return for now if widget does not exist yet
    if (!m_widget)
        return;

    if ( styleChanges & wxSTAY_ON_TOP )
    {
        gtk_window_set_keep_above(GTK_WINDOW(m_widget),
                                  m_windowStyle & wxSTAY_ON_TOP);
    }

    if ( styleChanges & wxFRAME_NO_TASKBAR )
    {
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(m_widget),
                                         m_windowStyle & wxFRAME_NO_TASKBAR);
    }
}

bool wxTopLevelWindowGTK::SetTransparent(wxByte alpha)
{
    if (m_widget == NULL)
        return false;
#if GTK_CHECK_VERSION(2,12,0)
#ifndef __WXGTK3__
    if (gtk_check_version(2,12,0) == NULL)
#endif
    {
#if GTK_CHECK_VERSION(3,8,0)
        if(gtk_check_version(3,8,0) == NULL)
        {
            gtk_widget_set_opacity(m_widget, alpha / 255.0);
        }
        else
#endif
        {
            // Can't avoid using this deprecated function with older GTK+.
            wxGCC_WARNING_SUPPRESS(deprecated-declarations);
            gtk_window_set_opacity(GTK_WINDOW(m_widget), alpha / 255.0);
            wxGCC_WARNING_RESTORE();
        }
        return true;
    }
#endif // GTK_CHECK_VERSION(2,12,0)
#ifndef __WXGTK3__
#ifdef GDK_WINDOWING_X11
    GdkWindow* window = gtk_widget_get_window(m_widget);
    if (window == NULL)
        return false;

    Display* dpy = GDK_WINDOW_XDISPLAY(window);
    Window win = GDK_WINDOW_XID(window);

    if (alpha == 0xff)
        XDeleteProperty(dpy, win, XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False));
    else
    {
        long opacity = alpha * 0x1010101L;
        XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False),
                        XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char *) &opacity, 1L);
    }
    XSync(dpy, False);
    return true;
#else // !GDK_WINDOWING_X11
    return false;
#endif // GDK_WINDOWING_X11 / !GDK_WINDOWING_X11
#endif // !__WXGTK3__
}

bool wxTopLevelWindowGTK::CanSetTransparent()
{
    // allow to override automatic detection as it's far from perfect
    const wxString SYSOPT_TRANSPARENT = "gtk.tlw.can-set-transparent";
    if ( wxSystemOptions::HasOption(SYSOPT_TRANSPARENT) )
    {
        return wxSystemOptions::GetOptionInt(SYSOPT_TRANSPARENT) != 0;
    }

#ifdef __WXGTK3__
    return gtk_widget_is_composited(m_widget) != 0;
#else
#if GTK_CHECK_VERSION(2,10,0)
    if (!gtk_check_version(2,10,0))
    {
        return gtk_widget_is_composited(m_widget) != 0;
    }
    else
#endif // In case of lower versions than gtk+-2.10.0 we could look for _NET_WM_CM_Sn ourselves
    {
        return false;
    }
#endif // !__WXGTK3__

#if 0 // Don't be optimistic here for the sake of wxAUI
    int opcode, event, error;
    // Check for the existence of a RGBA visual instead?
    return XQueryExtension(gdk_x11_get_default_xdisplay (),
                           "Composite", &opcode, &event, &error);
#endif
}
