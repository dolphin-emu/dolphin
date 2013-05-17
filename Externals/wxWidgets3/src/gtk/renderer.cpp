///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/renderer.cpp
// Purpose:     implementation of wxRendererNative for wxGTK
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.07.2003
// RCS-ID:      $Id: renderer.cpp 69741 2011-11-12 16:50:37Z PC $
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
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

#include "wx/renderer.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/module.h"
#endif

#include "wx/dcgraph.h"
#include "wx/gtk/dc.h"
#include "wx/gtk/private.h"

#include <gtk/gtk.h>

// ----------------------------------------------------------------------------
// wxRendererGTK: our wxRendererNative implementation
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxRendererGTK : public wxDelegateRendererNative
{
public:
    // draw the header control button (used by wxListCtrl)
    virtual int  DrawHeaderButton(wxWindow *win,
                                  wxDC& dc,
                                  const wxRect& rect,
                                  int flags = 0,
                                  wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
                                  wxHeaderButtonParams* params = NULL);

    virtual int GetHeaderButtonHeight(wxWindow *win);

    virtual int GetHeaderButtonMargin(wxWindow *win);


    // draw the expanded/collapsed icon for a tree control item
    virtual void DrawTreeItemButton(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0);

    virtual void DrawSplitterBorder(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0);
    virtual void DrawSplitterSash(wxWindow *win,
                                  wxDC& dc,
                                  const wxSize& size,
                                  wxCoord position,
                                  wxOrientation orient,
                                  int flags = 0);

    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0);

    virtual void DrawDropArrow(wxWindow *win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0);

    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0);

    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0);

    virtual void DrawItemSelectionRect(wxWindow *win,
                                       wxDC& dc,
                                       const wxRect& rect,
                                       int flags = 0);

    virtual void DrawChoice(wxWindow* win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags=0);

    virtual void DrawComboBox(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags=0);

    virtual void DrawTextCtrl(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags=0);

    virtual void DrawRadioBitmap(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags=0);

    virtual void DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags = 0);

    virtual wxSize GetCheckBoxSize(wxWindow *win);

    virtual wxSplitterRenderParams GetSplitterParams(const wxWindow *win);
};

// ============================================================================
// implementation
// ============================================================================

/* static */
wxRendererNative& wxRendererNative::GetDefault()
{
    static wxRendererGTK s_rendererGTK;

    return s_rendererGTK;
}

static GdkWindow* wxGetGdkWindowForDC(wxWindow* win, wxDC& dc)
{
    GdkWindow* gdk_window = NULL;

#if wxUSE_GRAPHICS_CONTEXT
    if ( dc.IsKindOf( CLASSINFO(wxGCDC) ) )
        gdk_window = win->GTKGetDrawingWindow();
    else
#endif
    {
#if wxUSE_NEW_DC
        wxDCImpl *impl = dc.GetImpl();
        wxGTKDCImpl *gtk_impl = wxDynamicCast( impl, wxGTKDCImpl );
        if (gtk_impl)
            gdk_window = gtk_impl->GetGDKWindow();
#else
        gdk_window = dc.GetGDKWindow();
#endif
    }

#if !wxUSE_GRAPHICS_CONTEXT
    wxUnusedVar(win);
#endif

    return gdk_window;
}

// ----------------------------------------------------------------------------
// list/tree controls drawing
// ----------------------------------------------------------------------------

int
wxRendererGTK::DrawHeaderButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags,
                                wxHeaderSortIconType sortArrow,
                                wxHeaderButtonParams* params)
{

    GtkWidget *button = wxGTKPrivate::GetHeaderButtonWidget();
    if (flags & wxCONTROL_SPECIAL)
        button = wxGTKPrivate::GetHeaderButtonWidgetFirst();
    if (flags & wxCONTROL_DIRTY)
        button = wxGTKPrivate::GetHeaderButtonWidgetLast();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    int x_diff = 0;
    if (win->GetLayoutDirection() == wxLayout_RightToLeft)
        x_diff = rect.width;

    GtkStateType state = GTK_STATE_NORMAL;
    if (flags & wxCONTROL_DISABLED)
        state = GTK_STATE_INSENSITIVE;
    else
    {
        if (flags & wxCONTROL_CURRENT)
            state = GTK_STATE_PRELIGHT;
    }

    gtk_paint_box
    (
        gtk_widget_get_style(button),
        gdk_window,
        state,
        GTK_SHADOW_OUT,
        NULL,
        button,
        "button",
        dc.LogicalToDeviceX(rect.x) - x_diff, rect.y, rect.width, rect.height
    );

    return DrawHeaderButtonContents(win, dc, rect, flags, sortArrow, params);
}

int wxRendererGTK::GetHeaderButtonHeight(wxWindow *WXUNUSED(win))
{
    GtkWidget *button = wxGTKPrivate::GetHeaderButtonWidget();

    GtkRequisition req;
    GTK_WIDGET_GET_CLASS(button)->size_request(button, &req);

    return req.height;
}

int wxRendererGTK::GetHeaderButtonMargin(wxWindow *WXUNUSED(win))
{
    wxFAIL_MSG( "GetHeaderButtonMargin() not implemented" );
    return -1;
}


// draw a ">" or "v" button
void
wxRendererGTK::DrawTreeItemButton(wxWindow* win,
                                  wxDC& dc, const wxRect& rect, int flags)
{
    GtkWidget *tree = wxGTKPrivate::GetTreeWidget();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    GtkStateType state;
    if ( flags & wxCONTROL_CURRENT )
        state = GTK_STATE_PRELIGHT;
    else
        state = GTK_STATE_NORMAL;

    int x_diff = 0;
    if (win->GetLayoutDirection() == wxLayout_RightToLeft)
        x_diff = rect.width;

    // x and y parameters specify the center of the expander
    gtk_paint_expander
    (
        gtk_widget_get_style(tree),
        gdk_window,
        state,
        NULL,
        tree,
        "treeview",
        dc.LogicalToDeviceX(rect.x) + rect.width / 2 - x_diff,
        dc.LogicalToDeviceY(rect.y) + rect.height / 2,
        flags & wxCONTROL_EXPANDED ? GTK_EXPANDER_EXPANDED
                                   : GTK_EXPANDER_COLLAPSED
    );
}


// ----------------------------------------------------------------------------
// splitter sash drawing
// ----------------------------------------------------------------------------

static int GetGtkSplitterFullSize(GtkWidget* widget)
{
    gint handle_size;
    gtk_widget_style_get(widget, "handle_size", &handle_size, NULL);

    return handle_size;
}

wxSplitterRenderParams
wxRendererGTK::GetSplitterParams(const wxWindow *WXUNUSED(win))
{
    // we don't draw any border, hence 0 for the second field
    return wxSplitterRenderParams
           (
               GetGtkSplitterFullSize(wxGTKPrivate::GetSplitterWidget()),
               0,
               true     // hot sensitive
           );
}

void
wxRendererGTK::DrawSplitterBorder(wxWindow * WXUNUSED(win),
                                  wxDC& WXUNUSED(dc),
                                  const wxRect& WXUNUSED(rect),
                                  int WXUNUSED(flags))
{
    // nothing to do
}

void
wxRendererGTK::DrawSplitterSash(wxWindow* win,
                                wxDC& dc,
                                const wxSize& size,
                                wxCoord position,
                                wxOrientation orient,
                                int flags)
{
    if (gtk_widget_get_window(win->m_wxwindow) == NULL)
    {
        // window not realized yet
        return;
    }

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    wxCoord full_size = GetGtkSplitterFullSize(wxGTKPrivate::GetSplitterWidget());

    // are we drawing vertical or horizontal splitter?
    const bool isVert = orient == wxVERTICAL;

    GdkRectangle rect;

    if ( isVert )
    {
        rect.x = position;
        rect.y = 0;
        rect.width = full_size;
        rect.height = size.y;
    }
    else // horz
    {
        rect.x = 0;
        rect.y = position;
        rect.height = full_size;
        rect.width = size.x;
    }

    int x_diff = 0;
    if (win->GetLayoutDirection() == wxLayout_RightToLeft)
        x_diff = rect.width;

    gtk_paint_handle
    (
        gtk_widget_get_style(win->m_wxwindow),
        gdk_window,
        flags & wxCONTROL_CURRENT ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
        GTK_SHADOW_NONE,
        NULL /* no clipping */,
        win->m_wxwindow,
        "paned",
        dc.LogicalToDeviceX(rect.x) - x_diff,
        dc.LogicalToDeviceY(rect.y),
        rect.width,
        rect.height,
        isVert ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL
    );
}

void
wxRendererGTK::DrawDropArrow(wxWindow* win,
                             wxDC& dc,
                             const wxRect& rect,
                             int flags)
{
    GtkWidget *button = wxGTKPrivate::GetButtonWidget();

    // If we give WX_PIZZA(win->m_wxwindow)->bin_window as
    // a window for gtk_paint_xxx function, then it won't
    // work for wxMemoryDC. So that is why we assume wxDC
    // is wxWindowDC (wxClientDC, wxMemoryDC and wxPaintDC
    // are derived from it) and use its m_window.
    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    // draw arrow so that there is even space horizontally
    // on both sides
    int arrowX = rect.width/4 + 1;
    int arrowWidth = rect.width - (arrowX*2);

    // scale arrow's height accoording to the width
    int arrowHeight = rect.width/3;
    int arrowY = (rect.height-arrowHeight)/2 +
                 ((rect.height-arrowHeight) & 1);

    GtkStateType state;

    if ( flags & wxCONTROL_PRESSED )
        state = GTK_STATE_ACTIVE;
    else if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;
    else if ( flags & wxCONTROL_CURRENT )
        state = GTK_STATE_PRELIGHT;
    else
        state = GTK_STATE_NORMAL;

    // draw arrow on button
    gtk_paint_arrow
    (
        gtk_widget_get_style(button),
        gdk_window,
        state,
        flags & wxCONTROL_PRESSED ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
        NULL,
        button,
        "arrow",
        GTK_ARROW_DOWN,
        FALSE,
        rect.x + arrowX,
        rect.y + arrowY,
        arrowWidth,
        arrowHeight
    );
}

void
wxRendererGTK::DrawComboBoxDropButton(wxWindow *win,
                                      wxDC& dc,
                                      const wxRect& rect,
                                      int flags)
{
    DrawPushButton(win,dc,rect,flags);
    DrawDropArrow(win,dc,rect);
}

wxSize
wxRendererGTK::GetCheckBoxSize(wxWindow *WXUNUSED(win))
{
    gint indicator_size, indicator_spacing;
    gtk_widget_style_get(wxGTKPrivate::GetCheckButtonWidget(),
                         "indicator_size", &indicator_size,
                         "indicator_spacing", &indicator_spacing,
                         NULL);

    int size = indicator_size + indicator_spacing * 2;
    return wxSize(size, size);
}

void
wxRendererGTK::DrawCheckBox(wxWindow* win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags )
{
    GtkWidget *button = wxGTKPrivate::GetCheckButtonWidget();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    gint indicator_size, indicator_spacing;
    gtk_widget_style_get(button,
                         "indicator_size", &indicator_size,
                         "indicator_spacing", &indicator_spacing,
                         NULL);

    GtkStateType state;

    if ( flags & wxCONTROL_PRESSED )
        state = GTK_STATE_ACTIVE;
    else if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;
    else if ( flags & wxCONTROL_CURRENT )
        state = GTK_STATE_PRELIGHT;
    else
        state = GTK_STATE_NORMAL;

    GtkShadowType shadow_type;

    if ( flags & wxCONTROL_UNDETERMINED )
        shadow_type = GTK_SHADOW_ETCHED_IN;
    else if ( flags & wxCONTROL_CHECKED )
        shadow_type = GTK_SHADOW_IN;
    else
        shadow_type = GTK_SHADOW_OUT;

    gtk_paint_check
    (
        gtk_widget_get_style(button),
        gdk_window,
        state,
        shadow_type,
        NULL,
        button,
        "cellcheck",
        dc.LogicalToDeviceX(rect.x) + indicator_spacing,
        dc.LogicalToDeviceY(rect.y) + indicator_spacing,
        indicator_size, indicator_size
    );
}

void
wxRendererGTK::DrawPushButton(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags)
{
    GtkWidget *button = wxGTKPrivate::GetButtonWidget();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    // draw button
    GtkStateType state;

    if ( flags & wxCONTROL_PRESSED )
        state = GTK_STATE_ACTIVE;
    else if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;
    else if ( flags & wxCONTROL_CURRENT )
        state = GTK_STATE_PRELIGHT;
    else
        state = GTK_STATE_NORMAL;

    gtk_paint_box
    (
        gtk_widget_get_style(button),
        gdk_window,
        state,
        flags & wxCONTROL_PRESSED ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
        NULL,
        button,
        "button",
        dc.LogicalToDeviceX(rect.x),
        dc.LogicalToDeviceY(rect.y),
        rect.width,
        rect.height
    );
}

void
wxRendererGTK::DrawItemSelectionRect(wxWindow* win,
                                     wxDC& dc,
                                     const wxRect& rect,
                                     int flags )
{
    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    if (flags & wxCONTROL_SELECTED)
    {
        int x_diff = 0;
        if (win->GetLayoutDirection() == wxLayout_RightToLeft)
            x_diff = rect.width;

        // the wxCONTROL_FOCUSED state is deduced
        // directly from the m_wxwindow by GTK+
        gtk_paint_flat_box(gtk_widget_get_style(wxGTKPrivate::GetTreeWidget()),
                        gdk_window,
                        GTK_STATE_SELECTED,
                        GTK_SHADOW_NONE,
                        NULL,
                        win->m_wxwindow,
                        "cell_even",
                        dc.LogicalToDeviceX(rect.x) - x_diff,
                        dc.LogicalToDeviceY(rect.y),
                        rect.width,
                        rect.height );
    }

    if ((flags & wxCONTROL_CURRENT) && (flags & wxCONTROL_FOCUSED))
        DrawFocusRect(win, dc, rect, flags);
}

void wxRendererGTK::DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );

    GtkStateType state;
    if (flags & wxCONTROL_SELECTED)
        state = GTK_STATE_SELECTED;
    else
        state = GTK_STATE_NORMAL;

    gtk_paint_focus( gtk_widget_get_style(win->m_widget),
                     gdk_window,
                     state,
                     NULL,
                     win->m_wxwindow,
                     NULL,
                     dc.LogicalToDeviceX(rect.x),
                     dc.LogicalToDeviceY(rect.y),
                     rect.width,
                     rect.height );
}

// Uses the theme to draw the border and fill for something like a wxTextCtrl
void wxRendererGTK::DrawTextCtrl(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    GtkWidget *entry = wxGTKPrivate::GetTextEntryWidget();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);

    GtkStateType state = GTK_STATE_NORMAL;
    if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;

    gtk_widget_set_can_focus(entry, (flags & wxCONTROL_CURRENT) != 0);

    gtk_paint_shadow
    (
        gtk_widget_get_style(entry),
        gdk_window,
        state,
        GTK_SHADOW_OUT,
        NULL,
        entry,
        "entry",
        dc.LogicalToDeviceX(rect.x),
        dc.LogicalToDeviceY(rect.y),
        rect.width,
        rect.height
  );
}

// Draw the equivalent of a wxComboBox
void wxRendererGTK::DrawComboBox(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    GtkWidget *combo = wxGTKPrivate::GetComboBoxWidget();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);

    GtkStateType state = GTK_STATE_NORMAL;
    if ( flags & wxCONTROL_DISABLED )
       state = GTK_STATE_INSENSITIVE;

    gtk_widget_set_can_focus(combo, (flags & wxCONTROL_CURRENT) != 0);

    gtk_paint_shadow
    (
        gtk_widget_get_style(combo),
        gdk_window,
        state,
        GTK_SHADOW_OUT,
        NULL,
        combo,
        "combobox",
        dc.LogicalToDeviceX(rect.x),
        dc.LogicalToDeviceY(rect.y),
        rect.width,
        rect.height
    );

    wxRect r = rect;
    int extent = rect.height / 2;
    r.x += rect.width - extent - extent/2;
    r.y += extent/2;
    r.width = extent;
    r.height = extent;

    gtk_paint_arrow
    (
        gtk_widget_get_style(combo),
        gdk_window,
        state,
        GTK_SHADOW_OUT,
        NULL,
        combo,
        "arrow",
        GTK_ARROW_DOWN,
        TRUE,
        dc.LogicalToDeviceX(r.x),
        dc.LogicalToDeviceY(r.y),
        r.width,
        r.height
    );

    r = rect;
    r.x += rect.width - 2*extent;
    r.width = 2;

    gtk_paint_box
    (
        gtk_widget_get_style(combo),
        gdk_window,
        state,
        GTK_SHADOW_ETCHED_OUT,
        NULL,
        combo,
        "vseparator",
        dc.LogicalToDeviceX(r.x),
        dc.LogicalToDeviceY(r.y+1),
        r.width,
        r.height-2
    );
}


void wxRendererGTK::DrawChoice(wxWindow* win, wxDC& dc,
                           const wxRect& rect, int flags)
{
    DrawComboBox( win, dc, rect, flags );
}


// Draw a themed radio button
void wxRendererGTK::DrawRadioBitmap(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    GtkWidget *button = wxGTKPrivate::GetRadioButtonWidget();

    GdkWindow* gdk_window = wxGetGdkWindowForDC(win, dc);

    GtkShadowType shadow_type = GTK_SHADOW_OUT;
    if ( flags & wxCONTROL_CHECKED )
        shadow_type = GTK_SHADOW_IN;
    else if ( flags & wxCONTROL_UNDETERMINED )
        shadow_type = GTK_SHADOW_ETCHED_IN;

    GtkStateType state = GTK_STATE_NORMAL;
    if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;
    if ( flags & wxCONTROL_PRESSED )
        state = GTK_STATE_ACTIVE;
/*
    Don't know when to set this
       state_type = GTK_STATE_PRELIGHT;
*/

    gtk_paint_option
    (
        gtk_widget_get_style(button),
        gdk_window,
        state,
        shadow_type,
        NULL,
        button,
        "radiobutton",
        dc.LogicalToDeviceX(rect.x),
        dc.LogicalToDeviceY(rect.y),
        rect.width, rect.height
    );
}
