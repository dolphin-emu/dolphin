/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dc.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __WXGTK3__

#include "wx/window.h"
#include "wx/dcclient.h"
#include "wx/dcmemory.h"
#include "wx/dcscreen.h"
#include "wx/icon.h"
#include "wx/gtk/dc.h"

#include <gtk/gtk.h>

wxGTKCairoDCImpl::wxGTKCairoDCImpl(wxDC* owner)
    : base_type(owner)
{
    m_width = 0;
    m_height = 0;
}

wxGTKCairoDCImpl::wxGTKCairoDCImpl(wxDC* owner, int)
    : base_type(owner, 0)
{
    m_width = 0;
    m_height = 0;
}

wxGTKCairoDCImpl::wxGTKCairoDCImpl(wxDC* owner, double scaleFactor)
    : base_type(owner, 0)
{
    m_width = 0;
    m_height = 0;
    m_contentScaleFactor = scaleFactor;
}

wxGTKCairoDCImpl::wxGTKCairoDCImpl(wxDC* owner, wxWindow* window)
    : base_type(owner, 0)
{
    m_window = window;
    m_font = window->GetFont();
    m_textForegroundColour = window->GetForegroundColour();
    m_textBackgroundColour = window->GetBackgroundColour();
    m_width = 0;
    m_height = 0;
    m_contentScaleFactor = window->GetContentScaleFactor();
}

void wxGTKCairoDCImpl::DoDrawBitmap(const wxBitmap& bitmap, int x, int y, bool useMask)
{
    wxCHECK_RET(IsOk(), "invalid DC");

    cairo_t* cr = NULL;
    if (m_graphicContext)
        cr = static_cast<cairo_t*>(m_graphicContext->GetNativeContext());
    if (cr)
    {
        cairo_save(cr);
        bitmap.Draw(cr, x, y, useMask, &m_textForegroundColour, &m_textBackgroundColour);
        cairo_restore(cr);
    }
}

void wxGTKCairoDCImpl::DoDrawIcon(const wxIcon& icon, int x, int y)
{
    DoDrawBitmap(icon, x, y, true);
}

#if wxUSE_IMAGE
bool wxDoFloodFill(wxDC* dc, int x, int y, const wxColour& col, wxFloodFillStyle style);

bool wxGTKCairoDCImpl::DoFloodFill(int x, int y, const wxColour& col, wxFloodFillStyle style)
{
    return wxDoFloodFill(GetOwner(), x, y, col, style);
}
#endif

wxBitmap wxGTKCairoDCImpl::DoGetAsBitmap(const wxRect* /*subrect*/) const
{
    wxFAIL_MSG("DoGetAsBitmap not implemented");
    return wxBitmap();
}

bool wxGTKCairoDCImpl::DoGetPixel(int x, int y, wxColour* col) const
{
    if (col)
    {
        cairo_t* cr = NULL;
        if (m_graphicContext)
            cr = static_cast<cairo_t*>(m_graphicContext->GetNativeContext());
        if (cr)
        {
            cairo_surface_t* surface = cairo_get_target(cr);
            x = LogicalToDeviceX(x);
            y = LogicalToDeviceY(y);
            GdkPixbuf* pixbuf = gdk_pixbuf_get_from_surface(surface, x, y, 1, 1);
            if (pixbuf)
            {
                const guchar* src = gdk_pixbuf_get_pixels(pixbuf);
                col->Set(src[0], src[1], src[2]);
                g_object_unref(pixbuf);
                return true;
            }
            *col = wxColour();
        }
    }
    return false;
}

void wxGTKCairoDCImpl::DoGetSize(int* width, int* height) const
{
    if (width)
        *width = m_width;
    if (height)
        *height = m_height;
}

bool wxGTKCairoDCImpl::DoStretchBlit(int xdest, int ydest, int dstWidth, int dstHeight, wxDC* source, int xsrc, int ysrc, int srcWidth, int srcHeight, wxRasterOperationMode rop, bool useMask, int xsrcMask, int ysrcMask)
{
    wxCHECK_MSG(IsOk(), false, "invalid DC");
    wxCHECK_MSG(source && source->IsOk(), false, "invalid source DC");

    cairo_t* cr = NULL;
    if (m_graphicContext)
        cr = static_cast<cairo_t*>(m_graphicContext->GetNativeContext());
    cairo_t* cr_src = NULL;
    wxGraphicsContext* gc_src = source->GetGraphicsContext();
    if (gc_src)
        cr_src = static_cast<cairo_t*>(gc_src->GetNativeContext());

    if (cr == NULL || cr_src == NULL)
        return false;

    const int xsrc_dev = source->LogicalToDeviceX(xsrc);
    const int ysrc_dev = source->LogicalToDeviceY(ysrc);

    cairo_surface_t* surface = cairo_get_target(cr_src);
    cairo_surface_flush(surface);
    cairo_save(cr);
    cairo_translate(cr, xdest, ydest);
    cairo_rectangle(cr, 0, 0, dstWidth, dstHeight);
    double sx, sy;
    source->GetUserScale(&sx, &sy);
    cairo_scale(cr, dstWidth / (sx * srcWidth), dstHeight / (sy * srcHeight));
    cairo_set_source_surface(cr, surface, -xsrc_dev, -ysrc_dev);
    const wxRasterOperationMode rop_save = m_logicalFunction;
    SetLogicalFunction(rop);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_surface_t* maskSurf = NULL;
    if (useMask)
    {
        const wxBitmap& bitmap = source->GetImpl()->GetSelectedBitmap();
        if (bitmap.IsOk())
        {
            wxMask* mask = bitmap.GetMask();
            if (mask)
                maskSurf = *mask;
        }
    }
    if (maskSurf)
    {
        int xsrcMask_dev = xsrc_dev;
        int ysrcMask_dev = ysrc_dev;
        if (xsrcMask != -1)
            xsrcMask_dev = source->LogicalToDeviceX(xsrcMask);
        if (ysrcMask != -1)
            ysrcMask_dev = source->LogicalToDeviceY(ysrcMask);
        cairo_clip(cr);
        cairo_mask_surface(cr, maskSurf, -xsrcMask_dev, -ysrcMask_dev);
    }
    else
    {
        cairo_fill(cr);
    }
    cairo_restore(cr);
    m_logicalFunction = rop_save;
    return true;
}

void* wxGTKCairoDCImpl::GetCairoContext() const
{
    cairo_t* cr = NULL;
    if (m_graphicContext)
        cr = static_cast<cairo_t*>(m_graphicContext->GetNativeContext());
    return cr;
}
//-----------------------------------------------------------------------------

wxWindowDCImpl::wxWindowDCImpl(wxWindowDC* owner, wxWindow* window)
    : base_type(owner, window)
{
    GtkWidget* widget = window->m_wxwindow;
    if (widget == NULL)
        widget = window->m_widget;
    GdkWindow* gdkWindow = NULL;
    if (widget)
    {
        gdkWindow = gtk_widget_get_window(widget);
        m_ok = true;
    }
    if (gdkWindow)
    {
        cairo_t* cr = gdk_cairo_create(gdkWindow);
        wxGraphicsContext* gc = wxGraphicsContext::CreateFromNative(cr);
        gc->EnableOffset(m_contentScaleFactor <= 1);
        SetGraphicsContext(gc);
        GtkAllocation a;
        gtk_widget_get_allocation(widget, &a);
        int x, y;
        if (gtk_widget_get_has_window(widget))
        {
            m_width = gdk_window_get_width(gdkWindow);
            m_height = gdk_window_get_height(gdkWindow);
            x = m_width - a.width;
            y = m_height - a.height;
        }
        else
        {
            m_width = a.width;
            m_height = a.height;
            x = a.x;
            y = a.y;
            cairo_rectangle(cr, a.x, a.y, a.width, a.height);
            cairo_clip(cr);
        }
        if (x || y)
            SetDeviceLocalOrigin(x, y);
    }
    else
        SetGraphicsContext(wxGraphicsContext::Create());
}
//-----------------------------------------------------------------------------

wxClientDCImpl::wxClientDCImpl(wxClientDC* owner, wxWindow* window)
    : base_type(owner, window)
{
    GtkWidget* widget = window->m_wxwindow;
    if (widget == NULL)
        widget = window->m_widget;
    GdkWindow* gdkWindow = NULL;
    if (widget)
    {
        gdkWindow = gtk_widget_get_window(widget);
        m_ok = true;
    }
    if (gdkWindow)
    {
        cairo_t* cr = gdk_cairo_create(gdkWindow);
        wxGraphicsContext* gc = wxGraphicsContext::CreateFromNative(cr);
        gc->EnableOffset(m_contentScaleFactor <= 1);
        SetGraphicsContext(gc);
        if (gtk_widget_get_has_window(widget))
        {
            m_width = gdk_window_get_width(gdkWindow);
            m_height = gdk_window_get_height(gdkWindow);
        }
        else
        {
            GtkAllocation a;
            gtk_widget_get_allocation(widget, &a);
            m_width = a.width;
            m_height = a.height;
            cairo_rectangle(cr, a.x, a.y, a.width, a.height);
            cairo_clip(cr);
            SetDeviceLocalOrigin(a.x, a.y);
        }
    }
    else
        SetGraphicsContext(wxGraphicsContext::Create());
}
//-----------------------------------------------------------------------------

wxPaintDCImpl::wxPaintDCImpl(wxPaintDC* owner, wxWindow* window)
    : base_type(owner, window)
{
    cairo_t* cr = window->GTKPaintContext();
    wxCHECK_RET(cr, "using wxPaintDC without being in a native paint event");
    GdkWindow* gdkWindow = gtk_widget_get_window(window->m_wxwindow);
    m_width = gdk_window_get_width(gdkWindow);
    m_height = gdk_window_get_height(gdkWindow);
    cairo_reference(cr);
    wxGraphicsContext* gc = wxGraphicsContext::CreateFromNative(cr);
    gc->EnableOffset(m_contentScaleFactor <= 1);
    SetGraphicsContext(gc);
}
//-----------------------------------------------------------------------------

wxScreenDCImpl::wxScreenDCImpl(wxScreenDC* owner)
    : base_type(owner, 0)
{
    GdkWindow* window = gdk_get_default_root_window();
    m_width = gdk_window_get_width(window);
    m_height = gdk_window_get_height(window);
    cairo_t* cr = gdk_cairo_create(window);
    wxGraphicsContext* gc = wxGraphicsContext::CreateFromNative(cr);
    gc->EnableOffset(m_contentScaleFactor <= 1);
    SetGraphicsContext(gc);
}
//-----------------------------------------------------------------------------

wxMemoryDCImpl::wxMemoryDCImpl(wxMemoryDC* owner)
    : base_type(owner)
{
    m_ok = false;
}

wxMemoryDCImpl::wxMemoryDCImpl(wxMemoryDC* owner, wxBitmap& bitmap)
    : base_type(owner, 0)
    , m_bitmap(bitmap)
{
    Setup();
}

wxMemoryDCImpl::wxMemoryDCImpl(wxMemoryDC* owner, wxDC*)
    : base_type(owner)
{
    m_ok = false;
}

wxBitmap wxMemoryDCImpl::DoGetAsBitmap(const wxRect* subrect) const
{
    return subrect ? m_bitmap.GetSubBitmap(*subrect) : m_bitmap;
}

void wxMemoryDCImpl::DoSelect(const wxBitmap& bitmap)
{
    m_bitmap = bitmap;
    Setup();
}

const wxBitmap& wxMemoryDCImpl::GetSelectedBitmap() const
{
    return m_bitmap;
}

wxBitmap& wxMemoryDCImpl::GetSelectedBitmap()
{
    return m_bitmap;
}

void wxMemoryDCImpl::Setup()
{
    wxGraphicsContext* gc = NULL;
    m_ok = m_bitmap.IsOk();
    if (m_ok)
    {
        m_width = int(m_bitmap.GetScaledWidth());
        m_height = int(m_bitmap.GetScaledHeight());
        m_contentScaleFactor = m_bitmap.GetScaleFactor();
        cairo_t* cr = m_bitmap.CairoCreate();
        gc = wxGraphicsContext::CreateFromNative(cr);
        gc->EnableOffset(m_contentScaleFactor <= 1);
    }
    SetGraphicsContext(gc);
}
//-----------------------------------------------------------------------------

wxGTKCairoDC::wxGTKCairoDC(cairo_t* cr, wxWindow* window)
    : base_type(new wxGTKCairoDCImpl(this, window->GetContentScaleFactor()))
{
    cairo_reference(cr);
    wxGraphicsContext* gc = wxGraphicsContext::CreateFromNative(cr);
    gc->EnableOffset(window->GetContentScaleFactor() <= 1);
    SetGraphicsContext(gc);
}

#else

#include "wx/gtk/dc.h"

//-----------------------------------------------------------------------------
// wxGTKDCImpl
//-----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxGTKDCImpl, wxDCImpl);

wxGTKDCImpl::wxGTKDCImpl( wxDC *owner )
   : wxDCImpl( owner )
{
    m_ok = FALSE;

    m_pen = *wxBLACK_PEN;
    m_font = *wxNORMAL_FONT;
    m_brush = *wxWHITE_BRUSH;
}

wxGTKDCImpl::~wxGTKDCImpl()
{
}

void wxGTKDCImpl::DoSetClippingRegion( wxCoord x, wxCoord y, wxCoord width, wxCoord height )
{
    m_clipping = TRUE;
    m_clipX1 = x;
    m_clipY1 = y;
    m_clipX2 = x + width;
    m_clipY2 = y + height;
}

// ---------------------------------------------------------------------------
// get DC capabilities
// ---------------------------------------------------------------------------

void wxGTKDCImpl::DoGetSizeMM( int* width, int* height ) const
{
    int w = 0;
    int h = 0;
    GetOwner()->GetSize( &w, &h );
    if (width) *width = int( double(w) / (m_userScaleX*m_mm_to_pix_x) );
    if (height) *height = int( double(h) / (m_userScaleY*m_mm_to_pix_y) );
}

// Resolution in pixels per logical inch
wxSize wxGTKDCImpl::GetPPI() const
{
    // TODO (should probably be pure virtual)
    return wxSize(0, 0);
}
#endif
