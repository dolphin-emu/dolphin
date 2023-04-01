/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/statbox.cpp
// Purpose:     wxStaticBox
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATBOX

#include "wx/statbox.h"
#include "wx/osx/private.h"

wxWidgetImplType* wxWidgetImpl::CreateGroupBox( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer, pos, size );

    wxMacControl* peer = new wxMacControl( wxpeer );
    OSStatus err = CreateGroupBoxControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds, CFSTR(""),
        true /*primary*/, peer->GetControlRefAddr() );
    verify_noerr( err );
    return peer;
}
#endif // wxUSE_STATBOX

