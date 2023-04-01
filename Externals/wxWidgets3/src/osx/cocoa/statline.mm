/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/statline.mm
// Purpose:     wxStaticLine class
// Author:      Stefan Csomor
// Created:     28.06.99
// Copyright:   (c) 2008 Stefan Csomor
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

wxWidgetImplType* wxWidgetImpl::CreateStaticLine( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSBox* v = [[wxNSBox alloc] initWithFrame:r];
    wxWidgetCocoaImpl* c = new wxWidgetCocoaImpl( wxpeer, v );
    return c;
}

#endif //wxUSE_STATLINE
