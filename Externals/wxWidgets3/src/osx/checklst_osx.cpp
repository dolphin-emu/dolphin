///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/checklst.cpp
// Purpose:     implementation of wxCheckListBox class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////
//
// new DataBrowser-based version


#include "wx/wxprec.h"

#if wxUSE_CHECKLISTBOX

#include "wx/checklst.h"

#ifndef WX_PRECOMP
    #include "wx/arrstr.h"
#endif

#include "wx/osx/private.h"

wxBEGIN_EVENT_TABLE(wxCheckListBox, wxListBox)
wxEND_EVENT_TABLE()

void wxCheckListBox::Init()
{
}

bool wxCheckListBox::Create(
    wxWindow *parent,
    wxWindowID id,
    const wxPoint &pos,
    const wxSize &size,
    const wxArrayString& choices,
    long style,
    const wxValidator& validator,
    const wxString &name )
{
    wxCArrayString chs( choices );

    return Create( parent, id, pos, size, chs.GetCount(), chs.GetStrings(), style, validator, name );
}

bool wxCheckListBox::Create(
   wxWindow *parent,
   wxWindowID id,
   const wxPoint& pos,
   const wxSize& size,
   int n,
   const wxString choices[],
   long style,
   const wxValidator& validator,
   const wxString& name )
{    
    
    wxASSERT_MSG( !(style & wxLB_MULTIPLE) || !(style & wxLB_EXTENDED),
                  wxT("only one of listbox selection modes can be specified") );

    if ( !wxCheckListBoxBase::Create( parent, id, pos, size, n, choices, style & ~(wxHSCROLL | wxVSCROLL), validator, name ) )
        return false;

    int colwidth = 30;
    // TODO adapt the width according to the window variant
    m_checkColumn = GetListPeer()->InsertCheckColumn(0, wxEmptyString, true, wxALIGN_CENTER, colwidth);

    return true;
}

// ----------------------------------------------------------------------------
// wxCheckListBox functions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxCheckListBox functions
// ----------------------------------------------------------------------------

bool wxCheckListBox::IsChecked(unsigned int n) const
{
    wxCHECK_MSG( IsValid(n), false,
                 wxT("invalid index in wxCheckListBox::IsChecked") );

    return m_checks[n] != 0;
}

void wxCheckListBox::Check(unsigned int n, bool check)
{
    wxCHECK_RET( IsValid(n),
                 wxT("invalid index in wxCheckListBox::Check") );

    // intermediate var is needed to avoid compiler warning with VC++
    bool isChecked = m_checks[n] != 0;
    if ( check != isChecked )
    {
        m_checks[n] = check;

        GetListPeer()->UpdateLine(n);
    }
}

void wxCheckListBox::GetValueCallback( unsigned int n, wxListWidgetColumn* col , wxListWidgetCellValue& value )
{
    if ( col == m_checkColumn )
        value.Check( IsChecked( n ) );
    else
        wxListBox::GetValueCallback( n, col, value );
}

void wxCheckListBox::SetValueCallback( unsigned int n, wxListWidgetColumn* col , wxListWidgetCellValue& value )
{
    if ( col == m_checkColumn )
    {
        Check( n, value.IsChecked() );

        wxCommandEvent event( wxEVT_CHECKLISTBOX, GetId() );
        event.SetInt( n );
        event.SetString( GetString( n ) );
        event.SetEventObject( this );
        HandleWindowEvent( event );
    }
}



// ----------------------------------------------------------------------------
// methods forwarded to wxListBox
// ----------------------------------------------------------------------------

void wxCheckListBox::OnItemInserted(unsigned int pos)
{
    wxListBox::OnItemInserted(pos);

    m_checks.Insert(false, pos );
}

void wxCheckListBox::DoDeleteOneItem(unsigned int n)
{
    wxListBox::DoDeleteOneItem(n);

    m_checks.RemoveAt(n);
}

void wxCheckListBox::DoClear()
{
    wxListBox::DoClear();

    m_checks.Empty();
}

#endif // wxUSE_CHECKLISTBOX
