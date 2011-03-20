///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/panel.cpp
// Purpose:     Implementation of wxMSW-specific wxPanel class.
// Author:      Vadim Zeitlin
// Created:     2011-03-18
// RCS-ID:      $Id: panel.cpp 67253 2011-03-20 00:00:49Z VZ $
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
    #include "wx/brush.h"
    #include "wx/panel.h"
#endif // WX_PRECOMP

// ============================================================================
// implementation
// ============================================================================

bool wxPanel::HasTransparentBackground()
{
    for ( wxWindow *win = GetParent(); win; win = win->GetParent() )
    {
        if ( win->MSWHasInheritableBackground() )
            return true;

        if ( win->IsTopLevel() )
            break;
    }

    return false;
}

void wxPanel::DoSetBackgroundBitmap(const wxBitmap& bmp)
{
    delete m_backgroundBrush;
    m_backgroundBrush = bmp.IsOk() ? new wxBrush(bmp) : NULL;
}

WXHBRUSH wxPanel::MSWGetCustomBgBrush()
{
    if ( m_backgroundBrush )
        return (WXHBRUSH)m_backgroundBrush->GetResourceHandle();

    return wxPanelBase::MSWGetCustomBgBrush();
}
