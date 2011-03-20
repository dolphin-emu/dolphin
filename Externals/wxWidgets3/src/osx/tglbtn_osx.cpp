/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/tglbtn_osx.cpp
// Purpose:     Definition of the wxToggleButton class, which implements a
//              toggle button under wxMac.
// Author:      Stefan Csomor
// Modified by:
// Created:     08.02.01
// RCS-ID:      $Id: tglbtn_osx.cpp 67243 2011-03-19 08:36:23Z SC $
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
#include "wx/bmpbuttn.h"    // for wxDEFAULT_BUTTON_MARGIN

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxToggleButton, wxControl)
wxDEFINE_EVENT( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEvent );

// ============================================================================
// implementation
// ============================================================================
// ----------------------------------------------------------------------------
// wxToggleButton
// ----------------------------------------------------------------------------

bool wxToggleButton::Create(wxWindow *parent, wxWindowID id,
                            const wxString& label,
                            const wxPoint& pos,
                            const wxSize& size, long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    DontCreatePeer();
    
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;

    m_labelOrig = m_label = label ;

    SetPeer(wxWidgetImpl::CreateToggleButton( this, parent, id, label, pos, size, style, GetExtraStyle() )) ;

    MacPostControlCreate(pos,size) ;

  return TRUE;
}

wxSize wxToggleButton::DoGetBestSize() const
{
    int wBtn = 70 ;
    int hBtn = 20 ;

    int lBtn = m_label.Length() * 8 + 12 ;
    if (lBtn > wBtn)
        wBtn = lBtn;

    return wxSize ( wBtn , hBtn ) ;
}

void wxToggleButton::SetValue(bool val)
{
    GetPeer()->SetValue( val ) ;
}

bool wxToggleButton::GetValue() const
{
    return GetPeer()->GetValue() ;
}

void wxToggleButton::Command(wxCommandEvent & event)
{
   SetValue((event.GetInt() != 0));
   ProcessCommand(event);
}

bool wxToggleButton::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    wxCommandEvent event(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_windowId);
    event.SetInt(GetValue());
    event.SetEventObject(this);
    ProcessCommand(event);
    return true ;
}

// ----------------------------------------------------------------------------
// wxBitmapToggleButton
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxBitmapToggleButton, wxControl)

bool wxBitmapToggleButton::Create(wxWindow *parent, wxWindowID id,
                            const wxBitmap& label,
                            const wxPoint& pos,
                            const wxSize& size, long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    DontCreatePeer();
    
    m_bitmap = label;

    m_marginX =
    m_marginY = wxDEFAULT_BUTTON_MARGIN;

    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;

    SetPeer(wxWidgetImpl::CreateBitmapToggleButton( this, parent, id, label, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate(pos,size) ;

    return TRUE;
}

wxSize wxBitmapToggleButton::DoGetBestSize() const
{
    if (!m_bitmap.IsOk())
       return wxSize(20,20);

    wxSize best;
    best.x = m_bitmap.GetWidth() + 2 * m_marginX;
    best.y = m_bitmap.GetHeight() + 2 * m_marginY;

    return best;
}

void wxBitmapToggleButton::SetValue(bool val)
{
    GetPeer()->SetValue( val ) ;
}

bool wxBitmapToggleButton::GetValue() const
{
    return GetPeer()->GetValue() ;
}

void wxBitmapToggleButton::Command(wxCommandEvent & event)
{
   SetValue((event.GetInt() != 0));
   ProcessCommand(event);
}

bool wxBitmapToggleButton::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    wxCommandEvent event(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_windowId);
    event.SetInt(GetValue());
    event.SetEventObject(this);
    ProcessCommand(event);
    return noErr ;
}

#endif // wxUSE_TOGGLEBTN

