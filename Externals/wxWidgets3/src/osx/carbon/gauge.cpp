/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/gauge.cpp
// Purpose:     wxGauge class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_GAUGE

#include "wx/gauge.h"

#include "wx/osx/private.h"

class wxMacGaugeCarbonControl : public wxMacControl
{
public :
    wxMacGaugeCarbonControl( wxWindowMac* peer ) : wxMacControl( peer )
    {
    }

    void SetMaximum(wxInt32 v)
    {
        // switch back to determinate mode if not there already
        if ( GetData<Boolean>( kControlNoPart, kControlProgressBarIndeterminateTag ) != false )
        {
             SetData<Boolean>( kControlNoPart, kControlProgressBarIndeterminateTag, (Boolean)false );
        }

        wxMacControl::SetMaximum( v ) ;
    }

    void SetValue(wxInt32 v)
    {
        // switch back to determinate mode if not there already
        if ( GetData<Boolean>( kControlNoPart, kControlProgressBarIndeterminateTag ) != false )
        {
            SetData<Boolean>( kControlNoPart, kControlProgressBarIndeterminateTag, (Boolean)false );
        }

        wxMacControl::SetValue( v ) ;

        // turn off animation in the unnecessary situations as this is consuming a lot of CPU otherwise
        Boolean shouldAnimate = ( v > 0 && v < GetMaximum() ) ;
        if ( GetData<Boolean>( kControlEntireControl, kControlProgressBarAnimatingTag ) != shouldAnimate )
        {
            SetData<Boolean>( kControlEntireControl, kControlProgressBarAnimatingTag, shouldAnimate ) ;
            if ( !shouldAnimate )
                SetNeedsDisplay(NULL) ;
        }
    }

    void PulseGauge()
    {
        if ( GetData<Boolean>( kControlNoPart, kControlProgressBarIndeterminateTag ) != true )
        {
            SetData<Boolean>( kControlNoPart, kControlProgressBarIndeterminateTag, true);
        }

        if ( GetData<Boolean>( kControlEntireControl, kControlProgressBarAnimatingTag ) != true )
        {
            SetData<Boolean>( kControlEntireControl, kControlProgressBarAnimatingTag, true ) ;
        }
    }
};


wxWidgetImplType* wxWidgetImpl::CreateGauge( wxWindowMac* wxpeer,
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
    Rect bounds = wxMacGetBoundsForControl( wxpeer, pos, size );
    wxMacGaugeCarbonControl* peer = new wxMacGaugeCarbonControl( wxpeer );
    OSStatus err = CreateProgressBarControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds,
        value, minimum, maximum, false /* not indeterminate */, peer->GetControlRefAddr() );
    verify_noerr( err );
    if ( value == 0 )
        peer->SetData<Boolean>( kControlEntireControl, kControlProgressBarAnimatingTag, (Boolean)false );
    return peer;
}

#endif // wxUSE_GAUGE

