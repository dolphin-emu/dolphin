///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/srchctrl_osx.cpp
// Purpose:     implements mac carbon wxSearchCtrl
// Author:      Vince Harron
// Created:     2006-02-19
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

#include "wx/osx/private.h"

wxBEGIN_EVENT_TABLE(wxSearchCtrl, wxSearchCtrlBase)
wxEND_EVENT_TABLE()

wxIMPLEMENT_DYNAMIC_CLASS(wxSearchCtrl, wxSearchCtrlBase);


#endif // wxUSE_NATIVE_SEARCH_CONTROL


// ----------------------------------------------------------------------------
// wxSearchCtrl creation
// ----------------------------------------------------------------------------

// creation
// --------

wxSearchCtrl::wxSearchCtrl()
{
    Init();
}

wxSearchCtrl::wxSearchCtrl(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxValidator& validator,
           const wxString& name)
{
    Init();

    Create(parent, id, value, pos, size, style, validator, name);
}

void wxSearchCtrl::Init()
{
#if wxUSE_MENUS
    m_menu = 0;
#endif
}

wxSearchWidgetImpl* wxSearchCtrl::GetSearchPeer() const
{
    return dynamic_cast<wxSearchWidgetImpl*> (GetPeer());
}

wxSearchCtrl::~wxSearchCtrl()
{
#if wxUSE_MENUS
    delete m_menu;
#endif
}

wxSize wxSearchCtrl::DoGetBestSize() const
{
    wxSize size = wxWindow::DoGetBestSize();
    // it seems to return a default width of about 16, which is way too small here.
    if (size.GetWidth() < 100)
        size.SetWidth(100);

    return size;
}

#if wxUSE_MENUS

// search control specific interfaces
// wxSearchCtrl owns menu after this call
void wxSearchCtrl::SetMenu( wxMenu* menu )
{
    if ( menu == m_menu )
    {
        // no change
        return;
    }

    if ( m_menu )
    {
        m_menu->SetInvokingWindow( 0 );
    }

    delete m_menu;
    m_menu = menu;

    if ( m_menu )
    {
        m_menu->SetInvokingWindow( this );
    }

    GetSearchPeer()->SetSearchMenu( m_menu );
}

wxMenu* wxSearchCtrl::GetMenu()
{
    return m_menu;
}

#endif  // wxUSE_MENUS

void wxSearchCtrl::ShowSearchButton( bool show )
{
    if ( IsSearchButtonVisible() == show )
    {
        // no change
        return;
    }
    GetSearchPeer()->ShowSearchButton( show );
}

bool wxSearchCtrl::IsSearchButtonVisible() const
{
    return GetSearchPeer()->IsSearchButtonVisible();
}


void wxSearchCtrl::ShowCancelButton( bool show )
{
    if ( IsCancelButtonVisible() == show )
    {
        // no change
        return;
    }
    GetSearchPeer()->ShowCancelButton( show );
}

bool wxSearchCtrl::IsCancelButtonVisible() const
{
    return GetSearchPeer()->IsCancelButtonVisible();
}

void wxSearchCtrl::SetDescriptiveText(const wxString& text)
{
    m_descriptiveText = text;
    GetSearchPeer()->SetDescriptiveText(text);
}

wxString wxSearchCtrl::GetDescriptiveText() const
{
    return m_descriptiveText;
}

bool wxSearchCtrl::Create(wxWindow *parent, wxWindowID id,
            const wxString& value,
            const wxPoint& pos,
            const wxSize& size,
            long style,
            const wxValidator& validator,
            const wxString& name)
{
    DontCreatePeer();
    m_editable = true ;

    if ( ! (style & wxNO_BORDER) )
        style = (style & ~wxBORDER_MASK) | wxSUNKEN_BORDER ;

    if ( !wxTextCtrlBase::Create( parent, id, pos, size, style & ~(wxHSCROLL | wxVSCROLL), validator, name ) )
        return false;

    if ( m_windowStyle & wxTE_MULTILINE )
    {
        // always turn on this style for multi-line controls
        m_windowStyle |= wxTE_PROCESS_ENTER;
        style |= wxTE_PROCESS_ENTER ;
    }


    SetPeer(wxWidgetImpl::CreateSearchControl( this, GetParent(), GetId(), value, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate(pos, size) ;

    // only now the embedding is correct and we can do a positioning update

    MacSuperChangedPosition() ;

    if ( m_windowStyle & wxTE_READONLY)
        SetEditable( false ) ;

    SetCursor( wxCursor( wxCURSOR_IBEAM ) ) ;

    return true;
}

bool wxSearchCtrl::HandleSearchFieldSearchHit()
{
    wxCommandEvent event(wxEVT_SEARCHCTRL_SEARCH_BTN, m_windowId );
    event.SetEventObject(this);

    // provide the string to search for directly in the event, this is more
    // convenient than retrieving it from the control in event handler code
    event.SetString(GetValue());

    return ProcessCommand(event);
}

bool wxSearchCtrl::HandleSearchFieldCancelHit()
{
    wxCommandEvent event(wxEVT_SEARCHCTRL_CANCEL_BTN, m_windowId );
    event.SetEventObject(this);
    return ProcessCommand(event);
}


#endif // wxUSE_SEARCHCTRL
