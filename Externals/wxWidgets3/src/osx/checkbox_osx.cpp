/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/checkbox_osx.cpp
// Purpose:     wxCheckBox
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHECKBOX

#include "wx/checkbox.h"
#include "wx/osx/private.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmapCheckBox, wxCheckBox);

// Single check box item
bool wxCheckBox::Create(wxWindow *parent,
    wxWindowID id,
    const wxString& label,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& validator,
    const wxString& name)
{    
    DontCreatePeer();
    
    if ( !wxCheckBoxBase::Create(parent, id, pos, size, style, validator, name) )
        return false;

    m_labelOrig = m_label = label ;

    WXValidateStyle( &style );
    SetPeer(wxWidgetImpl::CreateCheckBox( this, parent, id, label, pos, size, style, GetExtraStyle() )) ;

    MacPostControlCreate(pos, size) ;

    return true;
}


void wxCheckBox::SetValue(bool val)
{
    if (val)
        Set3StateValue(wxCHK_CHECKED);
    else
        Set3StateValue(wxCHK_UNCHECKED);
}

bool wxCheckBox::GetValue() const
{
    return (DoGet3StateValue() != 0);
}

void wxCheckBox::Command(wxCommandEvent & event)
{
    int state = event.GetInt();

    wxCHECK_RET( (state == wxCHK_UNCHECKED) || (state == wxCHK_CHECKED)
        || (state == wxCHK_UNDETERMINED),
        wxT("event.GetInt() returned an invalid checkbox state") );

    Set3StateValue((wxCheckBoxState)state);

    ProcessCommand(event);
}

wxCheckBoxState wxCheckBox::DoGet3StateValue() const
{
    return (wxCheckBoxState)GetPeer()->GetValue() ;
}

void wxCheckBox::DoSet3StateValue(wxCheckBoxState val)
{
    GetPeer()->SetValue( val ) ;
}

bool wxCheckBox::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    bool sendEvent = true;
    wxCheckBoxState newState = Get3StateValue();

    if ( !GetPeer()->ButtonClickDidStateChange() )
    {
        wxCheckBoxState origState ;

        origState = Get3StateValue();

        switch (origState)
        {
            case wxCHK_UNCHECKED:
                newState = wxCHK_CHECKED;
                break;

            case wxCHK_CHECKED:
                // If the style flag to allow the user setting the undetermined state is set,
                // then set the state to undetermined; otherwise set state to unchecked.
                newState = Is3rdStateAllowedForUser() ? wxCHK_UNDETERMINED : wxCHK_UNCHECKED;
                break;

            case wxCHK_UNDETERMINED:
                newState = wxCHK_UNCHECKED;
                break;

            default:
                break;
        }
        
        if (newState == origState)
            sendEvent = false;
        else
            Set3StateValue( newState );
    }
    else
    {
        // in case we cannot avoid this user state change natively (eg cocoa) we intercept it here
        if ( newState == wxCHK_UNDETERMINED && !Is3rdStateAllowedForUser() )
        {
            newState = wxCHK_CHECKED;
            Set3StateValue( newState );
        }
    }
    
    if (sendEvent)
    {
        wxCommandEvent event( wxEVT_CHECKBOX, m_windowId );
        event.SetInt( newState );
        event.SetEventObject( this );
        ProcessCommand( event );
    }

    return true;
}

// Bitmap checkbox
bool wxBitmapCheckBox::Create(wxWindow *parent,
    wxWindowID id,
    const wxBitmap *WXUNUSED(label),
    const wxPoint& WXUNUSED(pos),
    const wxSize& WXUNUSED(size),
    long style,
    const wxValidator& wxVALIDATOR_PARAM(validator),
    const wxString& name)
{
    SetName(name);
#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif
    m_windowStyle = style;

    if (parent)
        parent->AddChild(this);

    if ( id == -1 )
        m_windowId = NewControlId();
    else
        m_windowId = id;

    // TODO: Create the bitmap checkbox

    return false;
}

void wxBitmapCheckBox::SetLabel(const wxBitmap *WXUNUSED(bitmap))
{
    // TODO
    wxFAIL_MSG(wxT("wxBitmapCheckBox::SetLabel() not yet implemented"));
}

void wxBitmapCheckBox::SetSize(int x, int y, int width, int height, int sizeFlags)
{
    wxControl::SetSize( x , y , width , height , sizeFlags ) ;
}

void wxBitmapCheckBox::SetValue(bool WXUNUSED(val))
{
    // TODO
    wxFAIL_MSG(wxT("wxBitmapCheckBox::SetValue() not yet implemented"));
}

bool wxBitmapCheckBox::GetValue() const
{
    // TODO
    wxFAIL_MSG(wxT("wxBitmapCheckBox::GetValue() not yet implemented"));

    return false;
}

#endif
