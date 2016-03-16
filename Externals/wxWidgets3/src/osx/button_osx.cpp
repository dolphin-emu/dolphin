/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/button_osx.cpp
// Purpose:     wxButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/button.h"

#ifndef WX_PRECOMP
    #include "wx/panel.h"
    #include "wx/toplevel.h"
    #include "wx/dcclient.h"
    #include "wx/stattext.h"
#endif

#include "wx/stockitem.h"

#include "wx/osx/private.h"

namespace
{

// Returns true only if the id is wxID_HELP and the label is "Help" or empty.
bool IsHelpButtonWithStandardLabel(wxWindowID id, const wxString& label)
{
    if ( id != wxID_HELP )
        return false;

    if ( label.empty() )
        return true;

    const wxString labelText = wxStaticText::GetLabelText(label);
    return labelText == "Help" || labelText == _("Help");
}

} // anonymous namespace

bool wxButton::Create(wxWindow *parent,
    wxWindowID id,
    const wxString& labelOrig,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& validator,
    const wxString& name)
{
    // FIXME: this hack is needed because we're called from
    //        wxBitmapButton::Create() with this style and we currently use a
    //        different wxWidgetImpl method (CreateBitmapButton() rather than
    //        CreateButton()) for creating bitmap buttons, but we really ought
    //        to unify the creation of buttons of all kinds and then remove
    //        this check
    if ( style & wxBU_NOTEXT && !ShouldCreatePeer() )
    {
        return wxControl::Create(parent, id, pos, size, style,
                                 validator, name);
    }

    DontCreatePeer();

    m_marginX =
    m_marginY = 0;

    wxString label;

    if ( !(style & wxBU_NOTEXT) )
    {
        // Ignore the standard label for help buttons if possible, they use "?"
        // label under Mac which looks better.
        if ( !IsHelpButtonWithStandardLabel(id, labelOrig) )
        {
            label = labelOrig.empty() && wxIsStockID(id) ? wxGetStockLabel(id)
                                                         : labelOrig;
        }
    }


    if ( !wxButtonBase::Create(parent, id, pos, size, style, validator, name) )
        return false;

    m_labelOrig =
    m_label = label ;

    SetPeer(wxWidgetImpl::CreateButton( this, parent, id, label, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

    return true;
}

void wxButton::SetLabel(const wxString& label)
{
    if ( IsHelpButtonWithStandardLabel(GetId(), label) )
    {
        // ignore the standard label for the help buttons, it's not used
        return;
    }

    wxAnyButton::SetLabel(label);
#if wxOSX_USE_COCOA
    OSXUpdateAfterLabelChange(label);
#endif
}

wxWindow *wxButton::SetDefault()
{
    wxWindow *btnOldDefault = wxButtonBase::SetDefault();

    if ( btnOldDefault )
    {
        btnOldDefault->GetPeer()->SetDefaultButton( false );
    }

    GetPeer()->SetDefaultButton( true );

    return btnOldDefault;
}

void wxButton::Command (wxCommandEvent & WXUNUSED(event))
{
    GetPeer()->PerformClick() ;
    // ProcessCommand(event);
}

bool wxButton::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    wxCommandEvent event(wxEVT_BUTTON, m_windowId);
    event.SetEventObject(this);
    ProcessCommand(event);
    return true;
}

/* static */
wxSize wxButtonBase::GetDefaultSize()
{
    return wxAnyButton::GetDefaultSize();
}
