/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/button.cpp
// Purpose:     wxButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: button.cpp 67931 2011-06-14 13:00:42Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/button.h"

#ifndef WX_PRECOMP
    #include "wx/panel.h"
    #include "wx/toplevel.h"
    #include "wx/dcclient.h"
#endif

#include "wx/stockitem.h"

#include "wx/osx/private.h"

//
//
//

wxWidgetImplType* wxWidgetImpl::CreateButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    OSStatus err;
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* peer = new wxMacControl(wxpeer) ;
    if ( id == wxID_HELP )
    {
        ControlButtonContentInfo info ;
        info.contentType = kControlContentIconRef ;
        GetIconRef(kOnSystemDisk, kSystemIconsCreator, kHelpIcon, &info.u.iconRef);
        err = CreateRoundButtonControl(
            MAC_WXHWND(parent->MacGetTopLevelWindowRef()),
            &bounds, kControlRoundButtonNormalSize,
            &info, peer->GetControlRefAddr() );
    }
    else if ( label.Find('\n' ) == wxNOT_FOUND && label.Find('\r' ) == wxNOT_FOUND)
    {
        // Button height is static in Mac, can't be changed, so we need to force it here
        int maxHeight;
        switch (wxpeer->GetWindowVariant() )
        {
            default:
                wxFAIL_MSG( "unknown window variant" );
                // fall through

            case wxWINDOW_VARIANT_NORMAL:
            case wxWINDOW_VARIANT_LARGE:
                maxHeight = 20 ;
                break;
            case wxWINDOW_VARIANT_SMALL:
                maxHeight = 17;
                break;
            case wxWINDOW_VARIANT_MINI:
                maxHeight = 15;
        }
        bounds.bottom = bounds.top + maxHeight ;
        wxpeer->SetMaxSize( wxSize( wxpeer->GetMaxWidth() , maxHeight ));
        err = CreatePushButtonControl(
            MAC_WXHWND(parent->MacGetTopLevelWindowRef()),
            &bounds, CFSTR(""), peer->GetControlRefAddr() );
    }
    else
    {
        ControlButtonContentInfo info ;
        info.contentType = kControlNoContent ;
        err = CreateBevelButtonControl(
            MAC_WXHWND(parent->MacGetTopLevelWindowRef()) , &bounds, CFSTR(""),
            kControlBevelButtonLargeBevel, kControlBehaviorPushbutton,
            &info, 0, 0, 0, peer->GetControlRefAddr() );
    }
    verify_noerr( err );
    return peer;
}

void wxMacControl::SetDefaultButton( bool isDefault )
{
    SetData(kControlButtonPart , kControlPushButtonDefaultTag , (Boolean) isDefault ) ;
}

wxWidgetImplType* wxWidgetImpl::CreateDisclosureTriangle( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* peer = new wxMacControl(wxpeer) ;

    OSStatus err = CreateDisclosureTriangleControl(
            MAC_WXHWND(parent->MacGetTopLevelWindowRef()) , &bounds,
            kControlDisclosureTrianglePointDefault,
            wxCFStringRef( label ),
            0,    // closed
            TRUE, // draw title
            TRUE, // auto toggle back and forth
            peer->GetControlRefAddr() );

    verify_noerr( err );
    return peer;
}


