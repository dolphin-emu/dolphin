/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/choice_osx.cpp
// Purpose:     wxChoice
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: choice_osx.cpp 67343 2011-03-30 14:16:04Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHOICE

#include "wx/choice.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/dcclient.h"
#endif

#include "wx/osx/private.h"

wxChoice::~wxChoice()
{
    if ( HasClientObjectData() )
    {
        unsigned int i, max = GetCount();

        for ( i = 0; i < max; ++i )
            delete GetClientObject( i );
    }
    delete m_popUpMenu;
}

bool wxChoice::Create(wxWindow *parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    const wxArrayString& choices,
    long style,
    const wxValidator& validator,
    const wxString& name )
{
    if ( !Create( parent, id, pos, size, 0, NULL, style, validator, name ) )
        return false;

    Append( choices );

    if ( !choices.empty() )
        SetSelection( 0 );

    SetInitialSize( size );

    return true;
}

bool wxChoice::Create(wxWindow *parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    int n,
    const wxString choices[],
    long style,
    const wxValidator& validator,
    const wxString& name )
{    
    DontCreatePeer();
    
    if ( !wxChoiceBase::Create( parent, id, pos, size, style, validator, name ) )
        return false;

    m_popUpMenu = new wxMenu();
    m_popUpMenu->SetNoEventsMode(true);

    SetPeer(wxWidgetImpl::CreateChoice( this, parent, id, m_popUpMenu, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

#if !wxUSE_STD_CONTAINERS
    if ( style & wxCB_SORT )
        // autosort
        m_strings = wxArrayString( 1 );
#endif

    Append(n, choices);

    // Set the first item as being selected
    if (n > 0)
        SetSelection( 0 );

    // Needed because it is a wxControlWithItems
    SetInitialSize( size );

    return true;
}

// ----------------------------------------------------------------------------
// adding/deleting items to/from the list
// ----------------------------------------------------------------------------

int wxChoice::DoInsertItems(const wxArrayStringsAdapter & items,
                            unsigned int pos,
                            void **clientData, wxClientDataType type)
{
    const unsigned int numItems = items.GetCount();
    for( unsigned int i = 0; i < numItems; ++i, ++pos )
    {
        unsigned int idx;

#if wxUSE_STD_CONTAINERS
        if ( IsSorted() )
        {
            wxArrayString::iterator
                insertPoint = std::lower_bound( m_strings.begin(), m_strings.end(), items[i] );
            idx = insertPoint - m_strings.begin();
            m_strings.insert( insertPoint, items[i] );
        }
        else
#endif // wxUSE_STD_CONTAINERS
        {
            idx = pos;
            m_strings.Insert( items[i], idx );
        }

        wxString text = items[i];
        if (text == wxEmptyString)
            text = " ";  // menu items can't have empty labels
        m_popUpMenu->Insert( idx, i+1, text );
        m_datas.Insert( NULL, idx );
        AssignNewItemClientData(idx, clientData, i, type);
    }

    GetPeer()->SetMaximum( GetCount() );

    return pos - 1;
}

void wxChoice::DoDeleteOneItem(unsigned int n)
{
    wxCHECK_RET( IsValid(n) , wxT("wxChoice::Delete: invalid index") );

    if ( HasClientObjectData() )
        delete GetClientObject( n );

    m_popUpMenu->Delete( m_popUpMenu->FindItemByPosition( n ) );

    m_strings.RemoveAt( n ) ;
    m_datas.RemoveAt( n ) ;
    GetPeer()->SetMaximum( GetCount() ) ;

}

void wxChoice::DoClear()
{
    for ( unsigned int i = 0 ; i < GetCount() ; i++ )
    {
        m_popUpMenu->Delete( m_popUpMenu->FindItemByPosition( 0 ) );
    }

    m_strings.Empty() ;
    m_datas.Empty() ;

    GetPeer()->SetMaximum( 0 ) ;
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------
int wxChoice::GetSelection() const
{
    return GetPeer()->GetValue();
}

void wxChoice::SetSelection( int n )
{
    GetPeer()->SetValue( n );
}

// ----------------------------------------------------------------------------
// string list functions
// ----------------------------------------------------------------------------

unsigned int wxChoice::GetCount() const
{
    return m_strings.GetCount() ;
}

int wxChoice::FindString( const wxString& s, bool bCase ) const
{
#if !wxUSE_STD_CONTAINERS
    // Avoid assert for non-default args passed to sorted array Index
    if ( IsSorted() )
        bCase = true;
#endif

    return m_strings.Index( s , bCase ) ;
}

void wxChoice::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET( IsValid(n), wxT("wxChoice::SetString(): invalid index") );

    m_strings[n] = s ;

    m_popUpMenu->FindItemByPosition( n )->SetItemLabel( s ) ;
}

wxString wxChoice::GetString(unsigned int n) const
{
    wxCHECK_MSG( IsValid(n), wxEmptyString, wxT("wxChoice::GetString(): invalid index") );

    return m_strings[n] ;
}

// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------
void wxChoice::DoSetItemClientData(unsigned int n, void* clientData)
{
    m_datas[n] = (char*)clientData ;
}

void * wxChoice::DoGetItemClientData(unsigned int n) const
{
    return (void *)m_datas[n];
}

bool wxChoice::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    wxCommandEvent event( wxEVT_COMMAND_CHOICE_SELECTED, m_windowId );

    // actually n should be made sure by the os to be a valid selection, but ...
    int n = GetSelection();
    if ( n > -1 )
    {
        event.SetInt( n );
        event.SetString( GetStringSelection() );
        event.SetEventObject( this );

        if ( HasClientObjectData() )
            event.SetClientObject( GetClientObject( n ) );
        else if ( HasClientUntypedData() )
            event.SetClientData( GetClientData( n ) );

        ProcessCommand( event );
    }

    return true ;
}

wxSize wxChoice::DoGetBestSize() const
{
    int lbWidth = GetCount() > 0 ? 20 : 100;  // some defaults
    wxSize baseSize = wxWindow::DoGetBestSize();
    int lbHeight = baseSize.y;
    int wLine;

    {
        wxClientDC dc(const_cast<wxChoice*>(this));

        // Find the widest line
        for(unsigned int i = 0; i < GetCount(); i++)
        {
            wxString str(GetString(i));

            wxCoord width, height ;
            dc.GetTextExtent( str , &width, &height);
            wLine = width ;

            lbWidth = wxMax( lbWidth, wLine ) ;
        }

        // Add room for the popup arrow
        lbWidth += 2 * lbHeight ;

        wxCoord width, height ;
        dc.GetTextExtent( wxT("X"), &width, &height);
        int cx = width ;

        lbWidth += cx ;
    }

    return wxSize( lbWidth, lbHeight );
}

#endif // wxUSE_CHOICE
