///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/panelg.cpp
// Purpose:     Generic wxPanel implementation.
// Author:      Vadim Zeitlin
// Created:     2011-03-20
// RCS-ID:      $Id: panelg.cpp 67258 2011-03-20 11:50:47Z VZ $
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
    #include "wx/dc.h"
    #include "wx/panel.h"
#endif // WX_PRECOMP

#ifdef wxHAS_GENERIC_PANEL

// ============================================================================
// implementation
// ============================================================================

void wxPanel::DoSetBackgroundBitmap(const wxBitmap& bmp)
{
    m_bitmapBg = bmp;

    if ( m_bitmapBg.IsOk() )
    {
        Connect(wxEVT_ERASE_BACKGROUND,
                wxEraseEventHandler(wxPanel::OnEraseBackground));
    }
    else
    {
        Disconnect(wxEVT_ERASE_BACKGROUND,
                   wxEraseEventHandler(wxPanel::OnEraseBackground));
    }
}

void wxPanel::OnEraseBackground(wxEraseEvent& event)
{
    wxDC& dc = *event.GetDC();

    const wxSize clientSize = GetClientSize();
    const wxSize bitmapSize = m_bitmapBg.GetSize();

    for ( int x = 0; x < clientSize.x; x += bitmapSize.x )
    {
        for ( int y = 0; y < clientSize.y; y += bitmapSize.y )
        {
            dc.DrawBitmap(m_bitmapBg, x, y);
        }
    }
}

#endif // wxHAS_GENERIC_PANEL
