/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/stattext_osx.cpp
// Purpose:     wxStaticText
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: stattext_osx.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATTEXT

#include "wx/stattext.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

#include <stdio.h>


bool wxStaticText::Create( wxWindow *parent,
    wxWindowID id,
    const wxString& label,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name )
{    
    DontCreatePeer();
    
    if ( !wxControl::Create( parent, id, pos, size, style, wxDefaultValidator, name ) )
        return false;

    SetPeer(wxWidgetImpl::CreateStaticText( this, parent, id, label, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

    SetLabel(label);

    return true;
}

void wxStaticText::SetLabel(const wxString& label)
{
    m_labelOrig = label;

    // middle/end ellipsization is handled by the OS:
    if ( HasFlag(wxST_ELLIPSIZE_END) || HasFlag(wxST_ELLIPSIZE_MIDDLE)
#if wxOSX_USE_COCOA // Cocoa has all three modes
         || HasFlag(wxST_ELLIPSIZE_START)
#endif
    )
    {
        // leave ellipsization to the OS
        DoSetLabel(GetLabel());
    }
    else // not supported natively
    {
        DoSetLabel(GetEllipsizedLabel());
    }

    if ( !(GetWindowStyle() & wxST_NO_AUTORESIZE) &&
         !IsEllipsized() )  // don't resize if we adjust to current size
    {
        InvalidateBestSize();
        SetSize( GetBestSize() );
    }

    Refresh();

    // we shouldn't need forced updates
    // Update();
}

bool wxStaticText::SetFont(const wxFont& font)
{
    bool ret = wxControl::SetFont( font );

    if ( ret )
    {
        if ( !(GetWindowStyle() & wxST_NO_AUTORESIZE) )
        {
            InvalidateBestSize();
            SetSize( GetBestSize() );
        }
    }

    return ret;
}

void wxStaticText::DoSetLabel(const wxString& label)
{
    m_label = RemoveMnemonics(label);
    GetPeer()->SetLabel(m_label , GetFont().GetEncoding() );
}

#if wxUSE_MARKUP && wxOSX_USE_COCOA

bool wxStaticText::DoSetLabelMarkup(const wxString& markup)
{
    if ( !wxStaticTextBase::DoSetLabelMarkup(markup) )
        return false;

    GetPeer()->SetLabelMarkup(markup);

    return true;
}

#endif // wxUSE_MARKUP && wxOSX_USE_COCOA

wxString wxStaticText::DoGetLabel() const
{
    return m_label;
}

/*
   FIXME: UpdateLabel() should be called on size events when wxST_ELLIPSIZE_START is set
          to allow correct dynamic ellipsizing of the label
*/

#endif //if wxUSE_STATTEXT
