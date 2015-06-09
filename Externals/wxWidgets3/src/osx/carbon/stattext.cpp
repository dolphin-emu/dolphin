/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/stattext.cpp
// Purpose:     wxStaticText
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATTEXT

#include "wx/stattext.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

#include <stdio.h>

class wxMacStaticText : public wxMacControl
{
public:
    wxMacStaticText( wxWindowMac* peer ) : wxMacControl(peer)
    {
    }
    void SetLabel(const wxString& title, wxFontEncoding encoding)
    {
        wxCFStringRef str( title, encoding );
        OSStatus err = SetData<CFStringRef>(kControlEntireControl, kControlStaticTextCFStringTag, str);
        verify_noerr( err );
    }
};

wxSize wxStaticText::DoGetBestSize() const
{
    Point bounds;
#if wxOSX_USE_CARBON
    Rect bestsize = { 0 , 0 , 0 , 0 } ;

    // try the built-in best size if available
    Boolean former = GetPeer()->GetData<Boolean>( kControlStaticTextIsMultilineTag);
    GetPeer()->SetData( kControlStaticTextIsMultilineTag, (Boolean)0 );
    GetPeer()->GetBestRect( &bestsize ) ;
    GetPeer()->SetData( kControlStaticTextIsMultilineTag, former );
    
    if ( !EmptyRect( &bestsize ) )
    {
        bounds.h = bestsize.right - bestsize.left ;
        bounds.v = bestsize.bottom - bestsize.top ;
    }
    else
#endif
    {
#if wxOSX_USE_CARBON
        ControlFontStyleRec controlFont;
        OSStatus err = GetPeer()->GetData<ControlFontStyleRec>( kControlEntireControl, kControlFontStyleTag, &controlFont );
        verify_noerr( err );

#if wxOSX_USE_ATSU_TEXT
        SInt16 baseline;
        if ( m_font.MacGetThemeFontID() != kThemeCurrentPortFont )
        {
            // GetThemeTextDimensions will cache strings and the documentation
            // says not to use the NoCopy string creation calls.
            // This also means that we can't use CFSTR without
            // -fno-constant-cfstrings if the library might be unloaded,
            // as GetThemeTextDimensions may cache a pointer to our
            // unloaded segment.
            wxCFStringRef str( !m_label.empty() ? m_label : wxString(" "),
                              GetFont().GetEncoding() );

            err = GetThemeTextDimensions(
                (CFStringRef)str,
                m_font.MacGetThemeFontID(), kThemeStateActive, false, &bounds, &baseline );
            verify_noerr( err );
        }
        else
#endif
#endif
        {
            wxClientDC dc(const_cast<wxStaticText*>(this));
            wxCoord width, height ;
            dc.GetTextExtent( m_label , &width, &height);
            bounds.h = width;
            bounds.v = height;
        }

        if ( m_label.empty() )
            bounds.h = 0;
    }
    bounds.h += MacGetLeftBorderSize() + MacGetRightBorderSize();
    bounds.v += MacGetTopBorderSize() + MacGetBottomBorderSize();

    return wxSize( bounds.h, bounds.v );
}

/*
   FIXME: UpdateLabel() should be called on size events when wxST_ELLIPSIZE_START is set
          to allow correct dynamic ellipsizing of the label
*/

wxWidgetImplType* wxWidgetImpl::CreateStaticText( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer, pos, size );

    wxMacControl* peer = new wxMacStaticText( wxpeer );
    OSStatus err = CreateStaticTextControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()),
        &bounds, NULL, NULL, peer->GetControlRefAddr() );
    verify_noerr( err );

    if ( ( style & wxST_ELLIPSIZE_END ) || ( style & wxST_ELLIPSIZE_MIDDLE ) )
    {
        TruncCode tCode = truncEnd;
        if ( style & wxST_ELLIPSIZE_MIDDLE )
            tCode = truncMiddle;

        err = peer->SetData( kControlStaticTextTruncTag, tCode );
        err = peer->SetData( kControlStaticTextIsMultilineTag, (Boolean)0 );
    }
    return peer;
}

#endif //if wxUSE_STATTEXT
