/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/tglbtn_osx.cpp
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
#include "wx/bmpbuttn.h"    // for wxDEFAULT_BUTTON_MARGIN

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxToggleButton, wxControl);
wxDEFINE_EVENT( wxEVT_TOGGLEBUTTON, wxCommandEvent );

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
    
    m_marginX =
    m_marginY = 0;

    // FIXME: this hack is needed because we're called from
    //        wxBitmapToggleButton::Create() with this style and we currently use a
    //        different wxWidgetImpl method (CreateBitmapToggleButton() rather than
    //        CreateToggleButton()) for creating bitmap buttons, but we really ought
    //        to unify the creation of buttons of all kinds and then remove
    //        this check
    if ( style & wxBU_NOTEXT )
    {
        return wxControl::Create(parent, id, pos, size, style,
                                 validator, name);
    }

    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;

    m_labelOrig = m_label = label ;

    SetPeer(wxWidgetImpl::CreateToggleButton( this, parent, id, label, pos, size, style, GetExtraStyle() )) ;

    MacPostControlCreate(pos,size) ;

  return TRUE;
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
    wxCommandEvent event(wxEVT_TOGGLEBUTTON, m_windowId);
    event.SetInt(GetValue());
    event.SetEventObject(this);
    ProcessCommand(event);
    return true ;
}

// ----------------------------------------------------------------------------
// wxBitmapToggleButton
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmapToggleButton, wxToggleButton);

bool wxBitmapToggleButton::Create(wxWindow *parent, wxWindowID id,
                            const wxBitmap& label,
                            const wxPoint& pos,
                            const wxSize& size, long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    DontCreatePeer();
    
    if ( !wxToggleButton::Create(parent, id, wxEmptyString, pos, size, style | wxBU_NOTEXT | wxBU_EXACTFIT, validator, name) )
        return false;

    m_marginX =
    m_marginY = wxDEFAULT_BUTTON_MARGIN;

    m_bitmaps[State_Normal] = label;

    SetPeer(wxWidgetImpl::CreateBitmapToggleButton( this, parent, id, label, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate(pos,size) ;

    return TRUE;
}

#endif // wxUSE_TOGGLEBTN

