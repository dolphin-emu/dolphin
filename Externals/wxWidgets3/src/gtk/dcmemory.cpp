/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dcmemory.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/gtk/dcmemory.h"

#include <gtk/gtk.h>

//-----------------------------------------------------------------------------
// wxMemoryDCImpl
//-----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxMemoryDCImpl, wxWindowDCImpl);

wxMemoryDCImpl::wxMemoryDCImpl( wxMemoryDC *owner )
  : wxWindowDCImpl( owner )
{
    Init();
}

wxMemoryDCImpl::wxMemoryDCImpl( wxMemoryDC *owner, wxBitmap& bitmap)
  : wxWindowDCImpl( owner )
{
    Init();
    DoSelect(bitmap);
}

wxMemoryDCImpl::wxMemoryDCImpl( wxMemoryDC *owner, wxDC *WXUNUSED(dc) )
  : wxWindowDCImpl( owner )
{
    Init();
}

wxMemoryDCImpl::~wxMemoryDCImpl()
{
    g_object_unref(m_context);
}

void wxMemoryDCImpl::Init()
{
    m_ok = false;

    m_cmap = gtk_widget_get_default_colormap();

    m_context = gdk_pango_context_get();
    // Note: The Sun customised version of Pango shipping with Solaris 10
    // crashes if the language is left NULL (see bug 1374114)
    pango_context_set_language( m_context, gtk_get_default_language() );
    m_layout = pango_layout_new( m_context );
    m_fontdesc = pango_font_description_copy( pango_context_get_font_description( m_context ) );
}

void wxMemoryDCImpl::DoSelect( const wxBitmap& bitmap )
{
    Destroy();

    m_selected = bitmap;
    if (m_selected.IsOk())
    {
        m_gdkwindow = m_selected.GetPixmap();

        m_selected.PurgeOtherRepresentations(wxBitmap::Pixmap);

        SetUpDC( true );
    }
    else
    {
        m_ok = false;
        m_gdkwindow = NULL;
    }
}

void wxMemoryDCImpl::SetPen( const wxPen& penOrig )
{
    wxPen pen( penOrig );
    if ( m_selected.IsOk() &&
            m_selected.GetDepth() == 1 &&
                (pen != *wxTRANSPARENT_PEN) )
    {
        pen.SetColour( pen.GetColour() == *wxWHITE ? *wxBLACK : *wxWHITE );
    }

    wxWindowDCImpl::SetPen( pen );
}

void wxMemoryDCImpl::SetBrush( const wxBrush& brushOrig )
{
    wxBrush brush( brushOrig );
    if ( m_selected.IsOk() &&
            m_selected.GetDepth() == 1 &&
                (brush != *wxTRANSPARENT_BRUSH) )
    {
        brush.SetColour( brush.GetColour() == *wxWHITE ? *wxBLACK : *wxWHITE);
    }

    wxWindowDCImpl::SetBrush( brush );
}

void wxMemoryDCImpl::SetBackground( const wxBrush& brushOrig )
{
    wxBrush brush(brushOrig);

    if ( m_selected.IsOk() &&
            m_selected.GetDepth() == 1 &&
                (brush != *wxTRANSPARENT_BRUSH) )
    {
        brush.SetColour( brush.GetColour() == *wxWHITE ? *wxBLACK : *wxWHITE );
    }

    wxWindowDCImpl::SetBackground( brush );
}

void wxMemoryDCImpl::SetTextForeground( const wxColour& col )
{
    if ( m_selected.IsOk() && m_selected.GetDepth() == 1 )
        wxWindowDCImpl::SetTextForeground( col == *wxWHITE ? *wxBLACK : *wxWHITE);
    else
        wxWindowDCImpl::SetTextForeground( col );
}

void wxMemoryDCImpl::SetTextBackground( const wxColour &col )
{
    if (m_selected.IsOk() && m_selected.GetDepth() == 1)
        wxWindowDCImpl::SetTextBackground( col == *wxWHITE ? *wxBLACK : *wxWHITE );
    else
        wxWindowDCImpl::SetTextBackground( col );
}

void wxMemoryDCImpl::DoGetSize( int *width, int *height ) const
{
    if (m_selected.IsOk())
    {
        if (width) (*width) = m_selected.GetWidth();
        if (height) (*height) = m_selected.GetHeight();
    }
    else
    {
        if (width) (*width) = 0;
        if (height) (*height) = 0;
    }
}

wxBitmap wxMemoryDCImpl::DoGetAsBitmap(const wxRect *subrect) const
{
    wxBitmap bmp = GetSelectedBitmap();
    return subrect ? bmp.GetSubBitmap(*subrect) : bmp;
}

const wxBitmap& wxMemoryDCImpl::GetSelectedBitmap() const
{
    return m_selected;
}

wxBitmap& wxMemoryDCImpl::GetSelectedBitmap()
{
    return m_selected;
}

void* wxMemoryDCImpl::GetHandle() const
{
    const wxBitmap& bmp = GetSelectedBitmap();
    return bmp.GetPixmap();
}
