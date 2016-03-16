///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/nativewin.cpp
// Purpose:     wxNativeWindow implementation
// Author:      Vadim Zeitlin
// Created:     2008-03-05
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include "wx/nativewin.h"

#include <gtk/gtk.h>
#include "wx/gtk/private/gtk2-compat.h"

#ifdef GDK_WINDOWING_X11
    #include <gdk/gdkx.h>
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxNativeWindow
// ----------------------------------------------------------------------------

bool
wxNativeWindow::Create(wxWindow* parent,
                       wxWindowID winid,
                       wxNativeWindowHandle widget)
{
    wxCHECK_MSG( widget, false, wxS("Invalid null GtkWidget") );

    // Standard wxGTK controls use PreCreation() but we never have any size
    // specified at this stage, so don't bother with it.
    if ( !CreateBase(parent, winid) )
        return false;

    // Add a reference to the widget to match g_object_unref() in wxWindow dtor.
    m_widget = widget;
    g_object_ref(m_widget);

    parent->DoAddChild(this);

    PostCreation();

    // Ensure that the best (and minimal) size is set to fully display the
    // widget.
    GtkRequisition req;
    gtk_widget_get_preferred_size(widget, NULL, &req);
    SetInitialSize(wxSize(req.width, req.height));

    return true;
}

void wxNativeWindow::DoDisown()
{
    g_object_unref(m_widget);
}

// ----------------------------------------------------------------------------
// wxNativeContainerWindow
// ----------------------------------------------------------------------------

// TODO: we probably need equivalent code for other GDK platforms
#ifdef GDK_WINDOWING_X11

extern "C" GdkFilterReturn
wxNativeContainerWindowFilter(GdkXEvent *gdkxevent,
                              GdkEvent *event,
                              gpointer data)
{
    XEvent * const xevent = static_cast<XEvent *>(gdkxevent);
    if ( xevent->type == DestroyNotify )
    {
        // we won't need it any more
        gdk_window_remove_filter(event->any.window,
                                 wxNativeContainerWindowFilter, data);

        // the underlying window got destroyed, notify the C++ object
        static_cast<wxNativeContainerWindow *>(data)->OnNativeDestroyed();
    }

    return GDK_FILTER_CONTINUE;
}

#endif // GDK_WINDOWING_X11

bool wxNativeContainerWindow::Create(wxNativeContainerWindowHandle win)
{
    wxCHECK( win, false );

    if ( !wxTopLevelWindow::Create(NULL, wxID_ANY, "") )
        return false;

    // we need to realize the window first before reparenting it
    gtk_widget_realize(m_widget);
    gdk_window_reparent(gtk_widget_get_window(m_widget), win, 0, 0);

#ifdef GDK_WINDOWING_X11
    // if the native window is destroyed, our own window will be destroyed too
    // but GTK doesn't expect it and will complain about "unexpectedly
    // destroyed" GdkWindow, so intercept to DestroyNotify ourselves to fix
    // this and also destroy the associated C++ object when its window is
    // destroyed
    gdk_window_add_filter(gtk_widget_get_window(m_widget), wxNativeContainerWindowFilter, this);
#endif // GDK_WINDOWING_X11

    // we should be initially visible as we suppose that the native window we
    // wrap is (we could use gdk_window_is_visible() to test for this but this
    // doesn't make much sense unless we also react to visibility changes, so
    // just suppose it's always shown for now)
    Show();

    return true;
}

bool wxNativeContainerWindow::Create(wxNativeContainerWindowId anid)
{
    bool rc;
#ifdef __WXGTK3__
#ifdef GDK_WINDOWING_X11
    GdkWindow * const win = gdk_x11_window_foreign_new_for_display(gdk_display_get_default(), anid);
#else
    GdkWindow * const win = NULL;
#endif
#else
    GdkWindow * const win = gdk_window_foreign_new(anid);
#endif
    if ( win )
    {
        rc = Create(win);
        g_object_unref(win);
    }
    else // invalid native window id
    {
        rc = false;
    }

    return rc;
}

void wxNativeContainerWindow::OnNativeDestroyed()
{
    // unfortunately we simply can't do anything else than leak memory here:
    // we really need to call _gdk_window_destroy(m_widget->win, TRUE) to
    // indicate that the native window was deleted, but we can't do this
    // because it's a private GDK function and calling normal
    // gdk_window_destroy() results in X errors while nulling just the window
    // pointer and destroying m_widget results in many GTK errors
    GTKDisconnect(m_widget);
    m_widget = NULL;

    // notice that we intentionally don't use Close() nor Delete() here as our
    // window (and the windows of all of our children) is invalid any more and
    // any attempts to use it, as may happen with the delayed destruction, will
    // result in GDK warnings at best and crashes or X errors at worst
    delete this;
}

wxNativeContainerWindow::~wxNativeContainerWindow()
{
    // nothing to do here, either we have a valid m_widget and it will be
    // destroyed as usual (this corresponds to manual destruction of this C++
    // object) or we are being deleted because the native window was destroyed
    // and in this case our m_widget was set to NULL by OnNativeDestroyed()
}
