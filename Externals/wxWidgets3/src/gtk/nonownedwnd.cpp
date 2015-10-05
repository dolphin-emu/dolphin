///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/nonownedwnd.cpp
// Purpose:     wxGTK implementation of wxNonOwnedWindow.
// Author:      Vadim Zeitlin
// Created:     2011-10-12
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
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
    #include "wx/nonownedwnd.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/region.h"
#endif // WX_PRECOMP

#include "wx/graphics.h"

#include <gtk/gtk.h>
#include "wx/gtk/private/gtk2-compat.h"

// ----------------------------------------------------------------------------
// wxNonOwnedWindowShapeImpl: base class for region and path-based classes.
// ----------------------------------------------------------------------------

// This class provides behaviour common to both region and path-based
// implementations and defines SetShape() method and virtual dtor that can be
// called by wxNonOwnedWindow when it's realized leaving just the
// implementation of DoSetShape() to the derived classes.
class wxNonOwnedWindowShapeImpl : public wxEvtHandler
{
public:
    wxNonOwnedWindowShapeImpl(wxWindow* win) : m_win(win)
    {
    }

    virtual ~wxNonOwnedWindowShapeImpl() { }

    bool SetShape()
    {
        if ( m_win->m_wxwindow )
            SetShapeIfNonNull(gtk_widget_get_window(m_win->m_wxwindow));

        return SetShapeIfNonNull(gtk_widget_get_window(m_win->m_widget));
    }

    // Must be overridden to indicate if the data object must stay around or if
    // it can be deleted once SetShape() was called.
    virtual bool CanBeDeleted() const = 0;

protected:
    wxWindow* const m_win;

private:
    // SetShape to the given GDK window by calling DoSetShape() if it's non-NULL.
    bool SetShapeIfNonNull(GdkWindow* window)
    {
        return window && DoSetShape(window);
    }

    // SetShape the shape to the given GDK window which can be either the window
    // of m_widget or m_wxwindow of the wxWindow we're used with.
    virtual bool DoSetShape(GdkWindow* window) = 0;

    wxDECLARE_NO_COPY_CLASS(wxNonOwnedWindowShapeImpl);
};

// Version not using any custom shape.
class wxNonOwnedWindowShapeImplNone : public wxNonOwnedWindowShapeImpl
{
public:
    wxNonOwnedWindowShapeImplNone(wxWindow* win) :
        wxNonOwnedWindowShapeImpl(win)
    {
    }

    virtual bool CanBeDeleted() const wxOVERRIDE { return true; }

private:
    virtual bool DoSetShape(GdkWindow* window) wxOVERRIDE
    {
        gdk_window_shape_combine_region(window, NULL, 0, 0);

        return true;
    }
};

// Version using simple wxRegion.
class wxNonOwnedWindowShapeImplRegion : public wxNonOwnedWindowShapeImpl
{
public:
    wxNonOwnedWindowShapeImplRegion(wxWindow* win, const wxRegion& region) :
        wxNonOwnedWindowShapeImpl(win),
        m_region(region)
    {
    }

    virtual bool CanBeDeleted() const wxOVERRIDE { return true; }

private:
    virtual bool DoSetShape(GdkWindow* window) wxOVERRIDE
    {
        gdk_window_shape_combine_region(window, m_region.GetRegion(), 0, 0);

        return true;
    }

    wxRegion m_region;
};

#if wxUSE_GRAPHICS_CONTEXT

// Version using more complex wxGraphicsPath.
class wxNonOwnedWindowShapeImplPath : public wxNonOwnedWindowShapeImpl
{
public:
    wxNonOwnedWindowShapeImplPath(wxWindow* win, const wxGraphicsPath& path) :
        wxNonOwnedWindowShapeImpl(win),
        m_path(path),
        m_mask(CreateShapeBitmap(path), *wxBLACK)
    {

        m_win->Connect
               (
                wxEVT_PAINT,
                wxPaintEventHandler(wxNonOwnedWindowShapeImplPath::OnPaint),
                NULL,
                this
               );
    }

    virtual ~wxNonOwnedWindowShapeImplPath()
    {
        m_win->Disconnect
               (
                wxEVT_PAINT,
                wxPaintEventHandler(wxNonOwnedWindowShapeImplPath::OnPaint),
                NULL,
                this
               );
    }

    // Currently we always return false from here, if drawing the border
    // becomes optional, we could return true if we don't need to draw it.
    virtual bool CanBeDeleted() const wxOVERRIDE { return false; }

private:
    wxBitmap CreateShapeBitmap(const wxGraphicsPath& path)
    {
        // Draw the path on a bitmap to get the mask we need.
        //
        // Notice that using monochrome bitmap here doesn't work because of an
        // apparent wxGraphicsContext bug in wxGTK, so use a bitmap of screen
        // depth even if this is wasteful.
        wxBitmap bmp(m_win->GetSize());

        wxMemoryDC dc(bmp);

        dc.SetBackground(*wxBLACK);
        dc.Clear();

#ifdef __WXGTK3__
        wxGraphicsContext* context = dc.GetGraphicsContext();
#else
        wxScopedPtr<wxGraphicsContext> context(wxGraphicsContext::Create(dc));
#endif
        context->SetBrush(*wxWHITE);
        context->FillPath(path);

        return bmp;
    }

    virtual bool DoSetShape(GdkWindow *window) wxOVERRIDE
    {
        if (!m_mask)
            return false;

#ifdef __WXGTK3__
        cairo_region_t* region = gdk_cairo_region_create_from_surface(m_mask);
        gdk_window_shape_combine_region(window, region, 0, 0);
        cairo_region_destroy(region);
#else
        gdk_window_shape_combine_mask(window, m_mask, 0, 0);
#endif

        return true;
    }

    // Draw a shaped window border.
    void OnPaint(wxPaintEvent& event)
    {
        event.Skip();

        wxPaintDC dc(m_win);
#ifdef __WXGTK3__
        wxGraphicsContext* context = dc.GetGraphicsContext();
#else
        wxScopedPtr<wxGraphicsContext> context(wxGraphicsContext::Create(dc));
#endif
        context->SetPen(wxPen(*wxLIGHT_GREY, 2));
        context->StrokePath(m_path);
    }

    wxGraphicsPath m_path;
    wxMask m_mask;
};

#endif // wxUSE_GRAPHICS_CONTEXT

// ============================================================================
// wxNonOwnedWindow implementation
// ============================================================================

wxNonOwnedWindow::~wxNonOwnedWindow()
{
    delete m_shapeImpl;
}

void wxNonOwnedWindow::GTKHandleRealized()
{
    wxNonOwnedWindowBase::GTKHandleRealized();

    if ( m_shapeImpl )
    {
        m_shapeImpl->SetShape();

        // We can destroy wxNonOwnedWindowShapeImplRegion immediately but need
        // to keep wxNonOwnedWindowShapeImplPath around as it draws the border
        // on every repaint.
        if ( m_shapeImpl->CanBeDeleted() )
        {
            delete m_shapeImpl;
            m_shapeImpl = NULL;
        }
    }
}

bool wxNonOwnedWindow::DoClearShape()
{
    if ( !m_shapeImpl )
    {
        // Nothing to do, we don't have any custom shape.
        return true;
    }

    if ( gtk_widget_get_realized(m_widget) )
    {
        // Reset the existing shape immediately.
        wxNonOwnedWindowShapeImplNone data(this);
        data.SetShape();
    }
    //else: just do nothing, deleting m_shapeImpl is enough to ensure that we
    // don't set the custom shape later when we're realized.

    delete m_shapeImpl;
    m_shapeImpl = NULL;

    return true;
}

bool wxNonOwnedWindow::DoSetRegionShape(const wxRegion& region)
{
    // In any case get rid of the old data.
    delete m_shapeImpl;
    m_shapeImpl = NULL;

    if ( gtk_widget_get_realized(m_widget) )
    {
        // We can avoid an unnecessary heap allocation and just set the shape
        // immediately.
        wxNonOwnedWindowShapeImplRegion data(this, region);
        return data.SetShape();
    }
    else // Create an object that will set shape when we're realized.
    {
        m_shapeImpl = new wxNonOwnedWindowShapeImplRegion(this, region);

        // In general we don't know whether we are going to succeed or not, so
        // be optimistic.
        return true;
    }
}

#if wxUSE_GRAPHICS_CONTEXT

bool wxNonOwnedWindow::DoSetPathShape(const wxGraphicsPath& path)
{
    // The logic here is simpler than above because we always create
    // wxNonOwnedWindowShapeImplPath on the heap as we must keep it around,
    // even if we're already realized

    delete m_shapeImpl;
    m_shapeImpl = new wxNonOwnedWindowShapeImplPath(this, path);

    if ( gtk_widget_get_realized(m_widget) )
    {
        return m_shapeImpl->SetShape();
    }
    //else: will be done later from GTKHandleRealized().

    return true;
}

#endif // wxUSE_GRAPHICS_CONTEXT
