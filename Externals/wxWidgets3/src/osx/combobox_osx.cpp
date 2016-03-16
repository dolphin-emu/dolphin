/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/combobox_osx.cpp
// Purpose:     wxComboBox class using HIView ComboBox
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_COMBOBOX && wxOSX_USE_COCOA

#include "wx/combobox.h"
#include "wx/osx/private.h"

#ifndef WX_PRECOMP
#endif

// work in progress

wxComboBox::~wxComboBox()
{
}

bool wxComboBox::Create(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           const wxArrayString& choices,
           long style,
           const wxValidator& validator,
           const wxString& name)
{
    wxCArrayString chs( choices );

    return Create( parent, id, value, pos, size, chs.GetCount(),
                   chs.GetStrings(), style, validator, name );
}

bool wxComboBox::Create(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           int n, const wxString choices[],
           long style,
           const wxValidator& validator,
           const wxString& name)
{
    DontCreatePeer();
    
    m_text = NULL;
    m_choice = NULL;
    
    if ( !wxControl::Create( parent, id, pos, size, style, validator, name ) )
        return false;

    SetPeer(wxWidgetImpl::CreateComboBox( this, parent, id, NULL, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

    Append(n, choices);

    // Set up the initial value, by default the first item is selected.
    if ( !value.empty() )
        SetValue(value);
    else if (n > 0)
        SetSelection( 0 );

    // Needed because it is a wxControlWithItems
    SetInitialSize( size );

    return true;
}

void wxComboBox::DelegateTextChanged( const wxString& value )
{
    SetStringSelection( value );
}

void wxComboBox::DelegateChoice( const wxString& value )
{
    SetStringSelection( value );
}

int wxComboBox::DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type)
{
    const unsigned int numItems = items.GetCount();
    for( unsigned int i = 0; i < numItems; ++i, ++pos )
    {
        unsigned int idx;

        idx = pos;
        GetComboPeer()->InsertItem( idx, items[i] );

        if (idx > m_datas.GetCount())
            m_datas.SetCount(idx);
        m_datas.Insert( NULL, idx );
        AssignNewItemClientData(idx, clientData, i, type);
    }

    GetPeer()->SetMaximum( GetCount() );

    return pos - 1;
}

// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------
void wxComboBox::DoSetItemClientData(unsigned int n, void* clientData)
{
    m_datas[n] = (char*)clientData ;
}

void * wxComboBox::DoGetItemClientData(unsigned int n) const
{
    return (void *)m_datas[n];
}

unsigned int wxComboBox::GetCount() const
{
    return GetComboPeer()->GetNumberOfItems();
}

void wxComboBox::DoDeleteOneItem(unsigned int n)
{
    m_datas.RemoveAt(n);
    GetComboPeer()->RemoveItem(n);
}

void wxComboBox::DoClear()
{
    m_datas.Clear();
    GetComboPeer()->Clear();
}

void wxComboBox::GetSelection(long *from, long *to) const
{
    wxTextEntry::GetSelection(from, to);
}

int wxComboBox::GetSelection() const
{
    return GetComboPeer()->GetSelectedItem();
}

void wxComboBox::SetSelection(int n)
{
    GetComboPeer()->SetSelectedItem(n);
}

void wxComboBox::SetSelection(long from, long to)
{
    wxTextEntry::SetSelection(from, to);
}

int wxComboBox::FindString(const wxString& s, bool bCase) const
{
    if (!bCase)
    {
        for (unsigned i = 0; i < GetCount(); i++)
        {
            if (s.IsSameAs(GetString(i), false))
                return i;
        }
        return wxNOT_FOUND;
    }

    return GetComboPeer()->FindString(s);
}

wxString wxComboBox::GetString(unsigned int n) const
{
    wxCHECK_MSG( n < GetCount(), wxString(), "Invalid combobox index" );

    return GetComboPeer()->GetStringAtIndex(n);
}

wxString wxComboBox::GetStringSelection() const
{
    const int sel = GetSelection();
    return sel == wxNOT_FOUND ? wxString() : GetString(sel);
}

void wxComboBox::SetValue(const wxString& value)
{
    if ( HasFlag(wxCB_READONLY) )
        SetStringSelection( value ) ;
    else
        wxTextEntry::SetValue( value );
}

void wxComboBox::SetString(unsigned int n, const wxString& s)
{
    // Notice that we shouldn't delete and insert the item in this control
    // itself as this would also affect the client data which we need to
    // preserve here.
    GetComboPeer()->RemoveItem(n);
    GetComboPeer()->InsertItem(n, s);
    SetValue(s); // changing the item in the list won't update the display item
}

void wxComboBox::EnableTextChangedEvents(bool WXUNUSED(enable))
{
    // nothing to do here, events are never generated when we change the
    // control value programmatically anyhow by Cocoa
}

bool wxComboBox::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    wxCommandEvent event(wxEVT_COMBOBOX, m_windowId );
    event.SetInt(GetSelection());
    event.SetEventObject(this);
    event.SetString(GetStringSelection());
    ProcessCommand(event);
    return true;
}

wxComboWidgetImpl* wxComboBox::GetComboPeer() const
{
    return dynamic_cast<wxComboWidgetImpl*> (GetPeer());
}

void wxComboBox::Popup()
{
    GetComboPeer()->Popup();
}

void wxComboBox::Dismiss()
{
    GetComboPeer()->Dismiss();
}

#endif // wxUSE_COMBOBOX && wxOSX_USE_COCOA
