/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/statbox_osx.cpp
// Purpose:     wxStaticBox
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATBOX

#include "wx/statbox.h"
#include "wx/osx/private.h"

bool wxStaticBox::Create( wxWindow *parent,
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

    m_labelOrig = m_label = label;

    SetPeer(wxWidgetImpl::CreateGroupBox( this, parent, id, label, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

    return true;
}

void wxStaticBox::GetBordersForSizer(int *borderTop, int *borderOther) const
{
    static int extraTop = 11; 
    static int other = 11;

    *borderTop = extraTop;
    if ( !m_label.empty() )
    {
#if wxOSX_USE_COCOA
        *borderTop += 11;
#else
        *borderTop += GetCharHeight();
#endif
    }

    *borderOther = other;
}

bool wxStaticBox::SetFont(const wxFont& font)
{
    bool retval = wxWindowBase::SetFont( font );

    // dont' update the native control, it has its own small font

    return retval;
}

#endif // wxUSE_STATBOX

