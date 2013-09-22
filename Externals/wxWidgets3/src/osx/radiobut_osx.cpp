/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/radiobut.cpp
// Purpose:     wxRadioButton
// Author:      AUTHOR
// Modified by: JS Lair (99/11/15) adding the cyclic group notion for radiobox
// Created:     ??/??/98
// Copyright:   (c) AUTHOR
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_RADIOBTN

#include "wx/radiobut.h"
#include "wx/osx/private.h"

bool wxRadioButton::Create( wxWindow *parent,
    wxWindowID id,
    const wxString& label,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& validator,
    const wxString& name )
{    
    DontCreatePeer();
    
    if ( !wxControl::Create( parent, id, pos, size, style, validator, name ) )
        return false;

    m_labelOrig = m_label = label;

    SetPeer(wxWidgetImpl::CreateRadioButton( this, parent, id, label, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

    m_cycle = this;

    if (HasFlag( wxRB_GROUP ))
    {
        AddInCycle( NULL );
    }
    else
    {
        // search backward for last group start
        wxRadioButton *chief = NULL;
        wxWindowList::compatibility_iterator node = parent->GetChildren().GetLast();
        while (node)
        {
            wxWindow *child = node->GetData();
            if (child->IsKindOf( CLASSINFO( wxRadioButton ) ))
            {
                chief = (wxRadioButton*)child;
                if (child->HasFlag( wxRB_GROUP ))
                    break;
            }

            node = node->GetPrevious();
        }

        AddInCycle( chief );
    }

    return true;
}

wxRadioButton::~wxRadioButton()
{
    RemoveFromCycle();
}

void wxRadioButton::SetValue(bool val)
{
    wxRadioButton *cycle;
    if (GetPeer()->GetValue() == val)
        return;

    GetPeer()->SetValue( val );
    if (val)
    {
        cycle = this->NextInCycle();
        if (cycle != NULL)
        {
            while (cycle != this)
            {
                cycle->SetValue( false );
                cycle = cycle->NextInCycle();
            }
        }
    }
}

bool wxRadioButton::GetValue() const
{
    return GetPeer()->GetValue() != 0;
}

void wxRadioButton::Command(wxCommandEvent& event)
{
    SetValue( (event.GetInt() != 0) );
    ProcessCommand( event );
}

bool wxRadioButton::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    if ( !GetPeer()->ButtonClickDidStateChange() )
    {
        // if already set -> no action
        if (GetValue())
            return true;
    }

    wxRadioButton *cycle;
    cycle = this->NextInCycle();
    if (cycle != NULL)
    {
        while (cycle != this)
        {
            if (cycle->GetValue())
                cycle->SetValue( false );

            cycle = cycle->NextInCycle();
        }
    }

    SetValue( true );

    wxCommandEvent event2( wxEVT_RADIOBUTTON, m_windowId );
    event2.SetEventObject( this );
    event2.SetInt( true );
    ProcessCommand( event2 );

    return true;
}

wxRadioButton *wxRadioButton::AddInCycle(wxRadioButton *cycle)
{
    wxRadioButton *current;

    if (cycle == NULL)
    {
        m_cycle = this;
    }
    else
    {
        current = cycle;
        while (current->m_cycle != cycle)
            current = current->m_cycle;

        m_cycle = cycle;
        current->m_cycle = this;
    }

    return m_cycle;
}

void wxRadioButton::RemoveFromCycle()
{
    if ((m_cycle == NULL) || (m_cycle == this))
        return;

    // Find the previous one and make it point to the next one
    wxRadioButton* prev = this;
    while (prev->m_cycle != this)
        prev = prev->m_cycle;

    prev->m_cycle = m_cycle;
}

#endif
