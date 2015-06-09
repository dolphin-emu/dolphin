/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/tglbtn.cpp
// Purpose:     Definition of the wxToggleButton class, which implements a
//              toggle button under wxMac.
// Author:      Stefan Csomor
// Modified by:
// Created:     08.02.01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declatations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if wxUSE_TOGGLEBTN

#include "wx/tglbtn.h"
#include "wx/osx/private.h"
// Button

wxWidgetImplType* wxWidgetImpl::CreateToggleButton( wxWindowMac* wxpeer,
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
    verify_noerr ( CreateBevelButtonControl( MAC_WXHWND(parent->MacGetTopLevelWindowRef()) , &bounds , CFSTR("") ,
        kControlBevelButtonNormalBevel , kControlBehaviorToggles , NULL , 0 , 0 , 0 , peer->GetControlRefAddr() ) );
    return peer;
}

wxWidgetImplType* wxWidgetImpl::CreateBitmapToggleButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxBitmap& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* peer = new wxMacControl(wxpeer) ;

    ControlButtonContentInfo info;
    wxMacCreateBitmapButton( &info, label );
    verify_noerr ( CreateBevelButtonControl( MAC_WXHWND(parent->MacGetTopLevelWindowRef()) , &bounds , CFSTR("") ,
        kControlBevelButtonNormalBevel , kControlBehaviorOffsetContents | kControlBehaviorToggles , &info , 0 , 0 , 0 , peer->GetControlRefAddr() ) );

    wxMacReleaseBitmapButton( &info ) ;
    return peer;
}


#endif // wxUSE_TOGGLEBTN

