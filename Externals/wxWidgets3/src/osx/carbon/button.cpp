/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/button.cpp
// Purpose:     wxButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: button.cpp 67230 2011-03-18 14:20:12Z SC $
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

wxSize wxButton::DoGetBestSize() const
{
    if ( GetId() == wxID_HELP )
        return wxSize( 20 , 20 ) ;

    wxSize sz = GetDefaultSize() ;

    switch (GetWindowVariant())
    {
        case wxWINDOW_VARIANT_NORMAL:
        case wxWINDOW_VARIANT_LARGE:
            sz.y = 20 ;
            break;

        case wxWINDOW_VARIANT_SMALL:
            sz.y = 17 ;
            break;

        case wxWINDOW_VARIANT_MINI:
            sz.y = 15 ;
            break;

        default:
            break;
    }

#if wxOSX_USE_CARBON
    Rect    bestsize = { 0 , 0 , 0 , 0 } ;
    GetPeer()->GetBestRect( &bestsize ) ;

    int wBtn;
    if ( EmptyRect( &bestsize ) || ( GetWindowStyle() & wxBU_EXACTFIT) )
    {
        Point bounds;

        ControlFontStyleRec controlFont;
        OSStatus err = GetPeer()->GetData<ControlFontStyleRec>( kControlEntireControl, kControlFontStyleTag, &controlFont );
        verify_noerr( err );

        // GetThemeTextDimensions will cache strings and the documentation
        // says not to use the NoCopy string creation calls.
        // This also means that we can't use CFSTR without
        // -fno-constant-cfstrings if the library might be unloaded,
        // as GetThemeTextDimensions may cache a pointer to our
        // unloaded segment.
        wxCFStringRef str( !m_label.empty() ? m_label : wxString(" "),
                          GetFont().GetEncoding() );

#if wxOSX_USE_ATSU_TEXT
        SInt16 baseline;
        if ( m_font.MacGetThemeFontID() != kThemeCurrentPortFont )
        {
            err = GetThemeTextDimensions(
                (CFStringRef)str,
                m_font.MacGetThemeFontID(), kThemeStateActive, false, &bounds, &baseline );
            verify_noerr( err );
        }
        else
#endif
        {
            wxClientDC dc(const_cast<wxButton*>(this));
            wxCoord width, height ;
            dc.GetTextExtent( m_label , &width, &height);
            bounds.h = width;
            bounds.v = height;
        }

        wBtn = bounds.h + sz.y;
    }
    else
    {
        wBtn = bestsize.right - bestsize.left ;
        // non 'normal' window variants don't return the correct height
        // sz.y = bestsize.bottom - bestsize.top ;
    }
    if ((wBtn > sz.x) || ( GetWindowStyle() & wxBU_EXACTFIT))
        sz.x = wBtn;
#endif

    return sz ;
}

wxSize wxButton::GetDefaultSize()
{
    int wBtn = 70 ;
    int hBtn = 20 ;

    return wxSize(wBtn, hBtn);
}

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


