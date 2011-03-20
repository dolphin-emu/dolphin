///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/srchctrl.cpp
// Purpose:     implements mac carbon wxSearchCtrl
// Author:      Vince Harron
// Created:     2006-02-19
// RCS-ID:      $Id: srchctrl.cpp 66826 2011-02-02 07:55:57Z SC $
// Copyright:   Vince Harron
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SEARCHCTRL

#include "wx/srchctrl.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
#endif //WX_PRECOMP

#if wxUSE_NATIVE_SEARCH_CONTROL

#include "wx/osx/uma.h"
#include "wx/osx/carbon/private/mactext.h"

// ============================================================================
// wxMacSearchFieldControl
// ============================================================================

class wxMacSearchFieldControl : public wxMacUnicodeTextControl, public wxSearchWidgetImpl
{
public :
    wxMacSearchFieldControl( wxTextCtrl *wxPeer,
                             const wxString& str,
                             const wxPoint& pos,
                             const wxSize& size, long style ) ;

    // search field options
    virtual void ShowSearchButton( bool show );
    virtual bool IsSearchButtonVisible() const;

    virtual void ShowCancelButton( bool show );
    virtual bool IsCancelButtonVisible() const;

    virtual void SetSearchMenu( wxMenu* menu );

    virtual void SetDescriptiveText(const wxString& text);

    virtual bool SetFocus();

private:
} ;

static const EventTypeSpec eventList[] =
{
    { kEventClassSearchField, kEventSearchFieldCancelClicked } ,
    { kEventClassSearchField, kEventSearchFieldSearchClicked } ,
};

// ============================================================================
// implementation
// ============================================================================

static pascal OSStatus wxMacSearchControlEventHandler( EventHandlerCallRef WXUNUSED(handler) , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;

    wxMacCarbonEvent cEvent( event ) ;

    ControlRef controlRef ;
    wxSearchCtrl* thisWindow = (wxSearchCtrl*) data ;
    cEvent.GetParameter( kEventParamDirectObject , &controlRef ) ;

    switch( GetEventKind( event ) )
    {
        case kEventSearchFieldCancelClicked :
            thisWindow->HandleSearchFieldCancelHit() ;
            break ;
        case kEventSearchFieldSearchClicked :
            thisWindow->HandleSearchFieldSearchHit() ;
            break ;
    }

    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacSearchControlEventHandler )

wxMacSearchFieldControl::wxMacSearchFieldControl( wxTextCtrl *wxPeer,
                         const wxString& str,
                         const wxPoint& pos,
                         const wxSize& size, long style ) : wxMacUnicodeTextControl( wxPeer )
{
    m_font = wxPeer->GetFont() ;
    m_windowStyle = style ;
    m_selection.selStart = m_selection.selEnd = 0;
    Rect bounds = wxMacGetBoundsForControl( wxPeer , pos , size ) ;
    wxString st = str ;
    wxMacConvertNewlines10To13( &st ) ;
    wxCFStringRef cf(st , m_font.GetEncoding()) ;

    m_valueTag = kControlEditTextCFStringTag ;

    OptionBits attributes = kHISearchFieldAttributesSearchIcon;

    HIRect hibounds = { { bounds.left, bounds.top }, { bounds.right-bounds.left, bounds.bottom-bounds.top } };
    verify_noerr( HISearchFieldCreate(
        &hibounds,
        attributes,
        0, // MenuRef
        CFSTR(""),
        &m_controlRef
        ) );
    HIViewSetVisible (m_controlRef, true);

    verify_noerr( SetData<CFStringRef>( 0, kControlEditTextCFStringTag , cf ) ) ;

    ::InstallControlEventHandler( m_controlRef, GetwxMacSearchControlEventHandlerUPP(),
        GetEventTypeCount(eventList), eventList, wxPeer, NULL);
    SetNeedsFrame(false);
    wxMacUnicodeTextControl::InstallEventHandlers();
}

// search field options
void wxMacSearchFieldControl::ShowSearchButton( bool show )
{
    OptionBits set = 0;
    OptionBits clear = 0;
    if ( show )
    {
        set |= kHISearchFieldAttributesSearchIcon;
    }
    else
    {
        clear |= kHISearchFieldAttributesSearchIcon;
    }
    HISearchFieldChangeAttributes( m_controlRef, set, clear );
}

bool wxMacSearchFieldControl::IsSearchButtonVisible() const
{
    OptionBits attributes = 0;
    verify_noerr( HISearchFieldGetAttributes( m_controlRef, &attributes ) );
    return ( attributes & kHISearchFieldAttributesSearchIcon ) != 0;
}

void wxMacSearchFieldControl::ShowCancelButton( bool show )
{
    OptionBits set = 0;
    OptionBits clear = 0;
    if ( show )
    {
        set |= kHISearchFieldAttributesCancel;
    }
    else
    {
        clear |= kHISearchFieldAttributesCancel;
    }
    HISearchFieldChangeAttributes( m_controlRef, set, clear );
}

bool wxMacSearchFieldControl::IsCancelButtonVisible() const
{
    OptionBits attributes = 0;
    verify_noerr( HISearchFieldGetAttributes( m_controlRef, &attributes ) );
    return ( attributes & kHISearchFieldAttributesCancel ) != 0;
}

void wxMacSearchFieldControl::SetSearchMenu( wxMenu* menu )
{
    if ( menu )
    {
        verify_noerr( HISearchFieldSetSearchMenu( m_controlRef, MAC_WXHMENU(menu->GetHMenu()) ) );
    }
    else
    {
        verify_noerr( HISearchFieldSetSearchMenu( m_controlRef, 0 ) );
    }
}

void wxMacSearchFieldControl::SetDescriptiveText(const wxString& text)
{
    verify_noerr( HISearchFieldSetDescriptiveText(
                      m_controlRef,
                      wxCFStringRef( text, wxFont::GetDefaultEncoding() )));
}

bool wxMacSearchFieldControl::SetFocus()
{
    // NB: We have to implement SetFocus a little differently because kControlFocusNextPart
    // leads to setting the focus on the search icon rather than the text area.
    // We get around this by explicitly telling the control to set focus to the
    // text area.

    OSStatus err = SetKeyboardFocus( GetControlOwner( m_controlRef ), m_controlRef, kControlEditTextPart );
    if ( err == errCouldntSetFocus )
        return false ;
    SetUserFocusWindow(GetControlOwner( m_controlRef ) );
    return true;
}

wxWidgetImplType* wxWidgetImpl::CreateSearchControl( wxSearchCtrl* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& str,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    wxMacControl* peer = new wxMacSearchFieldControl( wxpeer , str , pos , size , style );

    return peer;
}

#endif // wxUSE_NATIVE_SEARCH_CONTROL

#endif // wxUSE_SEARCHCTRL
