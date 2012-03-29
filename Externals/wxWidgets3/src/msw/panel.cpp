///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/panel.cpp
// Purpose:     Implementation of wxMSW-specific wxPanel class.
// Author:      Vadim Zeitlin
// Created:     2011-03-18
// RCS-ID:      $Id: panel.cpp 69378 2011-10-11 17:07:43Z VZ $
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

