/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/textentry_osx.cpp
// Purpose:     wxTextEntry
// Author:      Stefan Csomor
// Modified by: Kevin Ollivier
// Created:     1998-01-01
// RCS-ID:      $Id: textentry_osx.cpp 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TEXTCTRL

#include "wx/textctrl.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/toplevel.h"
#endif

#ifdef __DARWIN__
    #include <sys/types.h>
    #include <sys/stat.h>
#else
    #include <stat.h>
#endif

#if wxUSE_STD_IOSTREAM
    #if wxUSE_IOSTREAMH
        #include <fstream.h>
    #else
        #include <fstream>
    #endif
#endif

#include "wx/filefn.h"
#include "wx/sysopt.h"
#include "wx/thread.h"

#include "wx/osx/private.h"

wxString wxTextEntry::DoGetValue() const
{
    return GetTextPeer()->GetStringValue() ;
}

void wxTextEntry::GetSelection(long* from, long* to) const
{
    GetTextPeer()->GetSelection( from , to ) ;
}

void wxTextEntry::SetMaxLength(unsigned long len)
{
    m_maxLength = len ;
}

// Clipboard operations

void wxTextEntry::Copy()
{
    if (CanCopy())
        GetTextPeer()->Copy() ;
}

void wxTextEntry::Cut()
{
    if (CanCut())
        GetTextPeer()->Cut() ;
}

void wxTextEntry::Paste()
{
    if (CanPaste())
        GetTextPeer()->Paste() ;
}

bool wxTextEntry::CanCopy() const
{
    // Can copy if there's a selection
    long from, to;
    GetSelection( &from, &to );

    return (from != to);
}

bool wxTextEntry::CanCut() const
{
    if ( !IsEditable() )
        return false;

    // Can cut if there's a selection
    long from, to;
    GetSelection( &from, &to );

    return (from != to);
}

bool wxTextEntry::CanPaste() const
{
    if (!IsEditable())
        return false;

    return GetTextPeer()->CanPaste() ;
}

void wxTextEntry::SetEditable(bool editable)
{
    if ( editable != m_editable )
    {
        m_editable = editable ;
        GetTextPeer()->SetEditable( editable ) ;
    }
}

void wxTextEntry::SetInsertionPoint(long pos)
{
    SetSelection( pos , pos ) ;
}

void wxTextEntry::SetInsertionPointEnd()
{
    long pos = GetLastPosition();
    SetInsertionPoint( pos );
}

long wxTextEntry::GetInsertionPoint() const
{
    long begin, end ;
    GetSelection( &begin , &end ) ;

    return begin ;
}

wxTextPos wxTextEntry::GetLastPosition() const
{
    return GetTextPeer()->GetLastPosition() ;
}

void wxTextEntry::Remove(long from, long to)
{
    {
        EventsSuppressor noevents(this);
        GetTextPeer()->Remove( from , to );
    }

    SendTextUpdatedEventIfAllowed();
}

void wxTextEntry::SetSelection(long from, long to)
{
    GetTextPeer()->SetSelection( from , to ) ;
}

void wxTextEntry::WriteText(const wxString& str)
{
    {
        EventsSuppressor noevents(this);
        GetTextPeer()->WriteText( str );
    }

    SendTextUpdatedEventIfAllowed();
}

void wxTextEntry::Clear()
{
    {
        EventsSuppressor noevents(this);
        GetTextPeer()->Clear();
    }

    SendTextUpdatedEventIfAllowed();
}

bool wxTextEntry::IsEditable() const
{
    return m_editable ;
}

// ----------------------------------------------------------------------------
// Undo/redo
// ----------------------------------------------------------------------------

void wxTextEntry::Undo()
{
    if (CanUndo())
        GetTextPeer()->Undo() ;
}

void wxTextEntry::Redo()
{
    if (CanRedo())
        GetTextPeer()->Redo() ;
}

bool wxTextEntry::CanUndo() const
{
    if ( !IsEditable() )
        return false ;

    return GetTextPeer()->CanUndo() ;
}

bool wxTextEntry::CanRedo() const
{
    if ( !IsEditable() )
        return false ;

    return GetTextPeer()->CanRedo() ;
}

wxTextWidgetImpl * wxTextEntry::GetTextPeer() const
{
    wxWindow * const win = const_cast<wxTextEntry *>(this)->GetEditableWindow();

    return win ? dynamic_cast<wxTextWidgetImpl *>(win->GetPeer()) : NULL;
}

#endif // wxUSE_TEXTCTRL
