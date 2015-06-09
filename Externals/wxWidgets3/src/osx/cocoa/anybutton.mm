/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/anybutton.mm
// Purpose:     wxAnyButton
// Author:      Stefan Csomor
// Created:     1998-01-01 (extracted from button.mm)
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/object.h"
#endif

#include "wx/button.h"

#include "wx/osx/private.h"

wxSize wxAnyButton::DoGetBestSize() const
{
    // We only use help button bezel if we don't have any (non standard) label
    // to display in the button. Otherwise even wxID_HELP buttons look like
    // normal push buttons.
    if ( GetId() == wxID_HELP && GetLabel().empty() )
        return wxSize( 23 , 23 ) ;

    wxRect r ;
    GetPeer()->GetBestRect(&r);

    wxSize sz = r.GetSize();
    sz.x  = sz.x  + MacGetLeftBorderSize() +
    MacGetRightBorderSize();
    sz.y = sz.y + MacGetTopBorderSize() +
    MacGetBottomBorderSize();

    const int wBtnStd = GetDefaultSize().x;

    if ( (sz.x < wBtnStd) && !HasFlag(wxBU_EXACTFIT) )
        sz.x = wBtnStd;

    return sz ;
}

wxSize wxAnyButton::GetDefaultSize()
{
    return wxSize(84, 20);
}
