/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/radiobut.cpp
// Purpose:     wxRadioButton
// Author:      Stefan Csomor
// Modified by: JS Lair (99/11/15) adding the cyclic group notion for radiobox
// Created:     01/01/98
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_RADIOBTN

#include "wx/radiobut.h"
#include "wx/osx/private.h"

wxWidgetImplType* wxWidgetImpl::CreateRadioButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* peer = new wxMacControl(wxpeer) ;
    OSStatus err = CreateRadioButtonControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds, CFSTR(""),
        0, false /* no autotoggle */, peer->GetControlRefAddr() );
    verify_noerr( err );

    return peer;
}

#endif
