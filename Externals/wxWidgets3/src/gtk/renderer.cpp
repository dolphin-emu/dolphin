///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/renderer.cpp
// Purpose:     implementation of wxRendererNative for wxGTK
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.07.2003
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
#ifndef __WXGTK3__
#include "wx/gtk/dc.h"
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"

#if defined(__WXGTK3__) && !GTK_CHECK_VERSION(3,14,0)
    #define GTK_STATE_FLAG_CHECKED (1 << 11)
#endif

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
                                  wxHeaderButtonParams* params = NULL) wxOVERRIDE;

    virtual int GetHeaderButtonHeight(wxWindow *win) wxOVERRIDE;

    virtual int GetHeaderButtonMargin(wxWindow *win) wxOVERRIDE;


    // draw the expanded/collapsed icon for a tree control item
    virtual void DrawTreeItemButton(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0) wxOVERRIDE;

    virtual void DrawSplitterBorder(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0) wxOVERRIDE;
    virtual void DrawSplitterSash(wxWindow *win,
                                  wxDC& dc,
                                  const wxSize& size,
                                  wxCoord position,
                                  wxOrientation orient,
                                  int flags = 0) wxOVERRIDE;

    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0) wxOVERRIDE;

    virtual void DrawDropArrow(wxWindow *win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0) wxOVERRIDE;

    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0) wxOVERRIDE;

    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0) wxOVERRIDE;

    virtual void DrawItemSelectionRect(wxWindow *win,
                                       wxDC& dc,
                                       const wxRect& rect,
                                       int flags = 0) wxOVERRIDE;

    virtual void DrawChoice(wxWindow* win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags=0) wxOVERRIDE;

    virtual void DrawComboBox(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags=0) wxOVERRIDE;

    virtual void DrawTextCtrl(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags=0) wxOVERRIDE;

    virtual void DrawRadioBitmap(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags=0) wxOVERRIDE;

    virtual void DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags = 0) wxOVERRIDE;

    virtual wxSize GetCheckBoxSize(wxWindow *win) wxOVERRIDE;

    virtual wxSplitterRenderParams GetSplitterParams(const wxWindow *win) wxOVERRIDE;
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

#ifdef __WXGTK3__
#define NULL_RECT
typedef cairo_t wxGTKDrawable;

static cairo_t* wxGetGTKDrawable(wxWindow*, const wxDC& dc)
{
    wxGraphicsContext* gc = dc.GetGraphicsContext();
    wxCHECK_MSG(gc, NULL, "cannot use wxRendererNative on wxDC of this type");
    return static_cast<cairo_t*>(gc->GetNativeContext());
}

static const GtkStateFlags stateTypeToFlags[] = {
    GTK_STATE_FLAG_NORMAL, GTK_STATE_FLAG_ACTIVE, GTK_STATE_FLAG_PRELIGHT,
    GTK_STATE_FLAG_SELECTED, GTK_STATE_FLAG_INSENSITIVE, GTK_STATE_FLAG_INCONSISTENT,
    GTK_STATE_FLAG_FOCUSED
};

#else
#define NULL_RECT NULL,
typedef GdkWindow wxGTKDrawable;

static GdkWindow* wxGetGTKDrawable(wxWindow* win, wxDC& dc)
{
    GdkWindow* gdk_window = NULL;

#if wxUSE_GRAPHICS_CONTEXT
    if ( wxDynamicCast(&dc, wxGCDC) )
        gdk_window = win->GTKGetDrawingWindow();
    else
#endif
    {
        wxDCImpl *impl = dc.GetImpl();
        wxGTKDCImpl *gtk_impl = wxDynamicCast( impl, wxGTKDCImpl );
        if (gtk_impl)
            gdk_window = gtk_impl->GetGDKWindow();
        else
            wxFAIL_MSG("cannot use wxRendererNative on wxDC of this type");
    }

#if !wxUSE_GRAPHICS_CONTEXT
    wxUnusedVar(win);
#endif

    return gdk_window;
}
#endif

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

#ifdef __WXGTK3__
    cairo_t* cr = wxGetGTKDrawable(win, dc);
    if (cr)
    {
        GtkStyleContext* sc = gtk_widget_get_style_context(button);
        gtk_style_context_save(sc);
        gtk_style_context_set_state(sc, stateTypeToFlags[state]);
        gtk_render_background(sc, cr, rect.x - x_diff+4, rect.y+4, rect.width-8, rect.height-8);
        gtk_render_frame(sc, cr, rect.x - x_diff+4, rect.y+4, rect.width-8, rect.height-8);
        gtk_style_context_restore(sc);
    }
#else
    GdkWindow* gdk_window = wxGetGTKDrawable(win, dc);
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
#endif

    return DrawHeaderButtonContents(win, dc, rect, flags, sortArrow, params);
}

int wxRendererGTK::GetHeaderButtonHeight(wxWindow *WXUNUSED(win))
{
    GtkWidget *button = wxGTKPrivate::GetHeaderButtonWidget();

    GtkRequisition req;
#ifdef __WXGTK3__
    gtk_widget_get_preferred_height(button, NULL, &req.height);
#else
    GTK_WIDGET_GET_CLASS(button)->size_request(button, &req);
#endif

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

    GtkStateType state;
    if ( flags & wxCONTROL_CURRENT )
        state = GTK_STATE_PRELIGHT;
    else
        state = GTK_STATE_NORMAL;

    int x_diff = 0;
    if (win->GetLayoutDirection() == wxLayout_RightToLeft)
        x_diff = rect.width;

#ifdef __WXGTK3__
    cairo_t* cr = wxGetGTKDrawable(win, dc);
    if (cr)
    {
        gtk_widget_set_state_flags(tree, stateTypeToFlags[state], true);
        GtkStyleContext* sc = gtk_widget_get_style_context(tree);
        gtk_render_expander(sc, cr, rect.x - x_diff, rect.y, rect.width, rect.height);
    }
#else
    // x and y parameters specify the center of the expander
    GdkWindow* gdk_window = wxGetGTKDrawable(win, dc);
    if (gdk_window == NULL)
        return;
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
#endif
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

    wxGTKDrawable* drawable = wxGetGTKDrawable(win, dc);
    if (drawable == NULL)
        return;

    // are we drawing vertical or horizontal splitter?
    const bool isVert = orient == wxVERTICAL;

    GtkWidget* widget = wxGTKPrivate::GetSplitterWidget(orient);
    const int full_size = GetGtkSplitterFullSize(widget);

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

#ifdef __WXGTK3__
    cairo_t* cr = wxGetGTKDrawable(win, dc);
    if (cr)
    {
        gtk_widget_set_state_flags(widget, stateTypeToFlags[flags & wxCONTROL_CURRENT ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL], true);
        GtkStyleContext* sc = gtk_widget_get_style_context(widget);
        gtk_render_handle(sc, cr, rect.x - x_diff, rect.y, rect.width, rect.height);
    }
#else
    GdkWindow* gdk_window = wxGetGTKDrawable(win, dc);
    if (gdk_window == NULL)
        return;
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
#endif
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

    // draw arrow so that there is even space horizontally
    // on both sides
    const int size = rect.width / 2;
    const int x = rect.x + (size + 1) / 2;
    const int y = rect.y + (rect.height - size + 1) / 2;

    GtkStateType state;

    if ( flags & wxCONTROL_PRESSED )
        state = GTK_STATE_ACTIVE;
    else if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;
    else if ( flags & wxCONTROL_CURRENT )
        state = GTK_STATE_PRELIGHT;
    else
        state = GTK_STATE_NORMAL;

#ifdef __WXGTK3__
    cairo_t* cr = wxGetGTKDrawable(win, dc);
    if (cr)
    {
        gtk_widget_set_state_flags(button, stateTypeToFlags[state], true);
        GtkStyleContext* sc = gtk_widget_get_style_context(button);
        gtk_render_arrow(sc, cr, G_PI, x, y, size);
    }
#else
    GdkWindow* gdk_window = wxGetGTKDrawable(win, dc);
    if (gdk_window == NULL)
        return;
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
        x, y,
        size, size
    );
#endif
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

    gint indicator_size, indicator_spacing;
    gtk_widget_style_get(button,
                         "indicator_size", &indicator_size,
                         "indicator_spacing", &indicator_spacing,
                         NULL);

#ifndef __WXGTK3__
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
#endif

#ifdef __WXGTK3__
    cairo_t* cr = wxGetGTKDrawable(win, dc);
    if (cr)
    {
        int stateFlags = GTK_STATE_FLAG_NORMAL;
        if (flags & wxCONTROL_CHECKED)
        {
            stateFlags = GTK_STATE_FLAG_ACTIVE;
            if (gtk_check_version(3,14,0) == NULL)
                stateFlags = GTK_STATE_FLAG_CHECKED;
        }
        if (flags & wxCONTROL_DISABLED)
            stateFlags |= GTK_STATE_FLAG_INSENSITIVE;
        if (flags & wxCONTROL_UNDETERMINED)
            stateFlags |= GTK_STATE_FLAG_INCONSISTENT;
        if (flags & wxCONTROL_CURRENT)
            stateFlags |= GTK_STATE_FLAG_PRELIGHT;
        GtkStyleContext* sc = gtk_widget_get_style_context(button);
        gtk_style_context_save(sc);
        gtk_style_context_set_state(sc, GtkStateFlags(stateFlags));
        gtk_style_context_add_class(sc, GTK_STYLE_CLASS_CHECK);
        gtk_render_check(sc, cr,
            rect.x + (rect.width - indicator_size) / 2,
            rect.y + (rect.height - indicator_size) / 2,
            indicator_size, indicator_size);
        gtk_style_context_restore(sc);
    }
#else
    GdkWindow* gdk_window = wxGetGTKDrawable(win, dc);
    if (gdk_window == NULL)
        return;

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
#endif
}

void
wxRendererGTK::DrawPushButton(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags)
{
    GtkWidget *button = wxGTKPrivate::GetButtonWidget();

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

#ifdef __WXGTK3__
    cairo_t* cr = wxGetGTKDrawable(win, dc);
    if (cr)
    {
        GtkStyleContext* sc = gtk_widget_get_style_context(button);
        gtk_style_context_save(sc);
        gtk_style_context_set_state(sc, stateTypeToFlags[state]);
        gtk_render_background(sc, cr, rect.x, rect.y, rect.width, rect.height);
        gtk_render_frame(sc, cr, rect.x, rect.y, rect.width, rect.height);
        gtk_style_context_restore(sc);
    }
#else
    GdkWindow* gdk_window = wxGetGTKDrawable(win, dc);
    if (gdk_window == NULL)
        return;

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
#endif
}

void
wxRendererGTK::DrawItemSelectionRect(wxWindow* win,
                                     wxDC& dc,
                                     const wxRect& rect,
                                     int flags )
{
    wxGTKDrawable* drawable = wxGetGTKDrawable(win, dc);
    if (drawable == NULL)
        return;

    if (flags & wxCONTROL_SELECTED)
    {
        int x_diff = 0;
        if (win->GetLayoutDirection() == wxLayout_RightToLeft)
            x_diff = rect.width;

        GtkWidget* treeWidget = wxGTKPrivate::GetTreeWidget();

#ifdef __WXGTK3__
        GtkStyleContext* sc = gtk_widget_get_style_context(treeWidget);
        gtk_style_context_save(sc);
        gtk_style_context_set_state(sc, GTK_STATE_FLAG_SELECTED);
        gtk_style_context_add_class(sc, GTK_STYLE_CLASS_CELL);
        gtk_render_background(sc, drawable, rect.x - x_diff, rect.y, rect.width, rect.height);
        gtk_style_context_restore(sc);
#else
        // the wxCONTROL_FOCUSED state is deduced
        // directly from the m_wxwindow by GTK+
        gtk_paint_flat_box(gtk_widget_get_style(treeWidget),
                        drawable,
                        GTK_STATE_SELECTED,
                        GTK_SHADOW_NONE,
                        NULL_RECT
                        win->m_wxwindow,
                        "cell_even",
                        dc.LogicalToDeviceX(rect.x) - x_diff,
                        dc.LogicalToDeviceY(rect.y),
                        rect.width,
                        rect.height );
#endif
    }

    if ((flags & wxCONTROL_CURRENT) && (flags & wxCONTROL_FOCUSED))
        DrawFocusRect(win, dc, rect, flags);
}

void wxRendererGTK::DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    wxGTKDrawable* drawable = wxGetGTKDrawable(win, dc);
    if (drawable == NULL)
        return;

    GtkStateType state;
    if (flags & wxCONTROL_SELECTED)
        state = GTK_STATE_SELECTED;
    else
        state = GTK_STATE_NORMAL;

#ifdef __WXGTK3__
    GtkStyleContext* sc = gtk_widget_get_style_context(win->m_widget);
    gtk_style_context_save(sc);
    gtk_style_context_set_state(sc, stateTypeToFlags[state]);
    gtk_render_focus(sc, drawable, rect.x, rect.y, rect.width, rect.height);
    gtk_style_context_restore(sc);
#else
    gtk_paint_focus( gtk_widget_get_style(win->m_widget),
                     drawable,
                     state,
                     NULL_RECT
                     win->m_wxwindow,
                     NULL,
                     dc.LogicalToDeviceX(rect.x),
                     dc.LogicalToDeviceY(rect.y),
                     rect.width,
                     rect.height );
#endif
}

// Uses the theme to draw the border and fill for something like a wxTextCtrl
void wxRendererGTK::DrawTextCtrl(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    wxGTKDrawable* drawable = wxGetGTKDrawable(win, dc);
    if (drawable == NULL)
        return;

    GtkWidget* entry = wxGTKPrivate::GetTextEntryWidget();

    GtkStateType state = GTK_STATE_NORMAL;
    if ( flags & wxCONTROL_DISABLED )
        state = GTK_STATE_INSENSITIVE;

    gtk_widget_set_can_focus(entry, (flags & wxCONTROL_CURRENT) != 0);

#ifdef __WXGTK3__
    GtkStyleContext* sc = gtk_widget_get_style_context(entry);
    gtk_style_context_save(sc);
    gtk_style_context_set_state(sc, stateTypeToFlags[state]);
    gtk_render_background(sc, drawable, rect.x, rect.y, rect.width, rect.height);
    gtk_render_frame(sc, drawable, rect.x, rect.y, rect.width, rect.height);
    gtk_style_context_restore(sc);
#else
    gtk_paint_shadow
    (
        gtk_widget_get_style(entry),
        drawable,
        state,
        GTK_SHADOW_OUT,
        NULL_RECT
        entry,
        "entry",
        dc.LogicalToDeviceX(rect.x),
        dc.LogicalToDeviceY(rect.y),
        rect.width,
        rect.height
  );
#endif
}

// Draw the equivalent of a wxComboBox
void wxRendererGTK::DrawComboBox(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    wxGTKDrawable* drawable = wxGetGTKDrawable(win, dc);
    if (drawable == NULL)
        return;

    GtkWidget* combo = wxGTKPrivate::GetComboBoxWidget();

    GtkStateType state = GTK_STATE_NORMAL;
    if ( flags & wxCONTROL_DISABLED )
       state = GTK_STATE_INSENSITIVE;

    gtk_widget_set_can_focus(combo, (flags & wxCONTROL_CURRENT) != 0);

#ifdef __WXGTK3__
    GtkStyleContext* sc = gtk_widget_get_style_context(combo);
    gtk_style_context_save(sc);
    gtk_style_context_set_state(sc, stateTypeToFlags[state]);
    gtk_render_background(sc, drawable, rect.x, rect.y, rect.width, rect.height);
    gtk_render_frame(sc, drawable, rect.x, rect.y, rect.width, rect.height);
    gtk_style_context_restore(sc);
    wxRect r = rect;
    r.x += r.width - r.height;
    r.width = r.height;
    DrawComboBoxDropButton(win, dc, r, flags);
#else
    gtk_paint_shadow
    (
        gtk_widget_get_style(combo),
        drawable,
        state,
        GTK_SHADOW_OUT,
        NULL_RECT
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
        drawable,
        state,
        GTK_SHADOW_OUT,
        NULL_RECT
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
        drawable,
        state,
        GTK_SHADOW_ETCHED_OUT,
        NULL_RECT
        combo,
        "vseparator",
        dc.LogicalToDeviceX(r.x),
        dc.LogicalToDeviceY(r.y+1),
        r.width,
        r.height-2
    );
#endif
}

void wxRendererGTK::DrawChoice(wxWindow* win, wxDC& dc,
                           const wxRect& rect, int flags)
{
    DrawComboBox( win, dc, rect, flags );
}


// Draw a themed radio button
void wxRendererGTK::DrawRadioBitmap(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    wxGTKDrawable* drawable = wxGetGTKDrawable(win, dc);
    if (drawable == NULL)
        return;

    GtkWidget* button = wxGTKPrivate::GetRadioButtonWidget();

#ifdef __WXGTK3__
    int state = GTK_STATE_FLAG_NORMAL;
    if (flags & wxCONTROL_CHECKED)
    {
        state = GTK_STATE_FLAG_ACTIVE;
        if (gtk_check_version(3,14,0) == NULL)
            state = GTK_STATE_FLAG_CHECKED;
    }
    else if (flags & wxCONTROL_UNDETERMINED)
        state = GTK_STATE_FLAG_INCONSISTENT;
    if (flags & wxCONTROL_DISABLED)
        state |= GTK_STATE_FLAG_INSENSITIVE;

    GtkStyleContext* sc = gtk_widget_get_style_context(button);
    gtk_style_context_save(sc);
    gtk_style_context_add_class(sc, GTK_STYLE_CLASS_RADIO);
    gtk_style_context_set_state(sc, GtkStateFlags(state));
    gtk_render_option(sc, drawable, rect.x, rect.y, rect.width, rect.height); 
    gtk_style_context_restore(sc);
#else
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
        drawable,
        state,
        shadow_type,
        NULL_RECT
        button,
        "radiobutton",
        dc.LogicalToDeviceX(rect.x),
        dc.LogicalToDeviceY(rect.y),
        rect.width, rect.height
    );
#endif
}
