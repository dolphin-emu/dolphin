/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/spinbutt.cpp
// Purpose:     wxSpinButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_SPINBTN

#include "wx/spinbutt.h"
#include "wx/osx/private.h"


wxWidgetImplType* wxWidgetImpl::CreateSpinButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    wxInt32 value,
                                    wxInt32 minimum,
                                    wxInt32 maximum,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size );

    wxMacControl* peer = new wxMacControl( wxpeer );
    OSStatus err = CreateLittleArrowsControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds, value,
        minimum, maximum, 1, peer->GetControlRefAddr() );
    verify_noerr( err );

    peer->SetActionProc( GetwxMacLiveScrollbarActionProc() );
    return peer ;
}

#endif // wxUSE_SPINBTN
