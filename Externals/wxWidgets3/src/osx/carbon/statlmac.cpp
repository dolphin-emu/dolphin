/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/statlmac.cpp
// Purpose:     a generic wxStaticLine class
// Author:      Vadim Zeitlin
// Created:     28.06.99
// Copyright:   (c) 1998 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STATLINE

#include "wx/statline.h"

#ifndef WX_PRECOMP
    #include "wx/statbox.h"
#endif

#include "wx/osx/private.h"

// ============================================================================
// implementation
// ============================================================================

wxWidgetImplType* wxWidgetImpl::CreateStaticLine( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* peer = new wxMacControl(wxpeer) ;
    verify_noerr(CreateSeparatorControl(MAC_WXHWND(parent->MacGetTopLevelWindowRef()),&bounds, peer->GetControlRefAddr() ) ) ;
    return peer;
}

#endif //wxUSE_STATLINE
