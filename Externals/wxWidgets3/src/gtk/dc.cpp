/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dc.cpp
// Purpose:
// Author:      Robert Roebling
// RCS-ID:      $Id: dc.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/gtk/dc.h"

//-----------------------------------------------------------------------------
// wxGTKDCImpl
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxGTKDCImpl, wxDCImpl)

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
