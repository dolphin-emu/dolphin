/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/statbmp.cpp
// Purpose:     wxStaticBitmap
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATBMP

#include "wx/statbmp.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
#endif

/*
 * wxStaticBitmap
 */

BEGIN_EVENT_TABLE(wxStaticBitmap, wxStaticBitmapBase)
    EVT_PAINT(wxStaticBitmap::OnPaint)
END_EVENT_TABLE()

bool wxStaticBitmap::Create(wxWindow *parent, wxWindowID id,
           const wxBitmap& bitmap,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxString& name)
{
    SetName(name);

    m_backgroundColour = parent->GetBackgroundColour() ;
    m_foregroundColour = parent->GetForegroundColour() ;

    m_bitmap = bitmap;
    if ( id == wxID_ANY )
          m_windowId = (int)NewControlId();
    else
        m_windowId = id;

    m_windowStyle = style;

    bool ret = wxControl::Create( parent, id, pos, size, style , wxDefaultValidator , name );
    SetInitialSize( size ) ;

    return ret;
}

void wxStaticBitmap::SetBitmap(const wxBitmap& bitmap)
{
    m_bitmap = bitmap;
    InvalidateBestSize();
    SetSize(GetBestSize());
    Refresh() ;
}

void wxStaticBitmap::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    wxPaintDC dc(this);
    PrepareDC(dc);

    if (m_bitmap.IsOk())
    {
        dc.DrawBitmap( m_bitmap , 0 , 0 , TRUE ) ;
    }
}

wxSize wxStaticBitmap::DoGetBestSize() const
{
    if ( m_bitmap.IsOk() )
        return DoGetSizeFromClientSize( wxSize(m_bitmap.GetWidth(), m_bitmap.GetHeight()) );

    // this is completely arbitrary
    return DoGetSizeFromClientSize( wxSize(16, 16) );
}

#endif
