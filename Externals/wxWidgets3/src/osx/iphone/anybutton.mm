/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/anybutton.mm
// Purpose:     wxAnyButton
// Author:      Stefan Csomor
// Created:     1998-01-01 (extracted from button.mm)
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/anybutton.h"

#ifndef WX_PRECOMP
    #include "wx/panel.h"
    #include "wx/toplevel.h"
    #include "wx/dcclient.h"
#endif

#include "wx/stockitem.h"

#include "wx/osx/private.h"

wxSize wxAnyButton::DoGetBestSize() const
{
    if ( GetId() == wxID_HELP )
        return wxSize( 18 , 18 ) ;

    wxSize sz = GetDefaultSize() ;

    wxRect r ;

    GetPeer()->GetBestRect(&r);

    if ( r.GetWidth() == 0 && r.GetHeight() == 0 )
    {
    }
    sz.x = r.GetWidth();
    sz.y = r.GetHeight();

    int wBtn = 72;

    if ((wBtn > sz.x) || ( GetWindowStyle() & wxBU_EXACTFIT))
        sz.x = wBtn;

    return sz ;
}

wxSize wxAnyButton::GetDefaultSize()
{
    int wBtn = 72 ;
    int hBtn = 35 ;

    return wxSize(wBtn, hBtn);
}
