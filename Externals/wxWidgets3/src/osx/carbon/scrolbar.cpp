/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/scrolbar.cpp
// Purpose:     wxScrollBar
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/scrolbar.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/osx/private.h"

class wxOSXScrollBarCarbonImpl : public wxMacControl
{
public :
    wxOSXScrollBarCarbonImpl( wxWindowMac* peer) : wxMacControl( peer )
    {
    }

    void    SetScrollThumb( wxInt32 value, wxInt32 thumbSize )
    {
        SetValue( value );
        SetControlViewSize(m_controlRef , thumbSize );
    }
protected:
};

wxWidgetImplType* wxWidgetImpl::CreateScrollBar( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer, pos, size );

    wxMacControl* peer = new wxOSXScrollBarCarbonImpl( wxpeer );
    OSStatus err = CreateScrollBarControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds,
        0, 0, 100, 1, true /* liveTracking */,
        GetwxMacLiveScrollbarActionProc(),
        peer->GetControlRefAddr() );
    verify_noerr( err );
    return peer;
}
