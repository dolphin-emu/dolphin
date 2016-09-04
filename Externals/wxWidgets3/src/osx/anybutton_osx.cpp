/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/anybutton_osx.cpp
// Purpose:     wxAnyButton
// Author:      Stefan Csomor
// Created:     1998-01-01 (extracted from button_osx.cpp)
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/anybutton.h"

#ifndef WX_PRECOMP
    #include "wx/panel.h"
    #include "wx/toplevel.h"
    #include "wx/dcclient.h"
    #include "wx/stattext.h"
#endif

#include "wx/stockitem.h"

#include "wx/osx/private.h"

wxBEGIN_EVENT_TABLE(wxAnyButton, wxControl)
    EVT_ENTER_WINDOW(wxAnyButton::OnEnterWindow)
    EVT_LEAVE_WINDOW(wxAnyButton::OnLeaveWindow)
wxEND_EVENT_TABLE()

void wxAnyButton::SetLabel(const wxString& label)
{
    if ( HasFlag(wxBU_NOTEXT) )
    {
        // just store the label internally but don't really use it for the
        // button
        m_labelOrig =
        m_label = label;
        return;
    }

    wxAnyButtonBase::SetLabel(label);
}

wxBitmap wxAnyButton::DoGetBitmap(State which) const
{
    return m_bitmaps[which];
}

void wxAnyButton::DoSetBitmap(const wxBitmap& bitmap, State which)
{
    m_bitmaps[which] = bitmap;

    if ( which == State_Normal )
        GetPeer()->SetBitmap(bitmap);
    else if ( which == State_Pressed )
    {
        wxButtonImpl* bi = dynamic_cast<wxButtonImpl*> (GetPeer());
        if ( bi )
            bi->SetPressedBitmap(bitmap);
    }
    InvalidateBestSize();
}

void wxAnyButton::DoSetBitmapPosition(wxDirection dir)
{
    GetPeer()->SetBitmapPosition(dir);
    InvalidateBestSize();
}

#if wxUSE_MARKUP && wxOSX_USE_COCOA

bool wxAnyButton::DoSetLabelMarkup(const wxString& markup)
{
    if ( !wxAnyButtonBase::DoSetLabelMarkup(markup) )
        return false;

    GetPeer()->SetLabelMarkup(markup);

    return true;
}

#endif // wxUSE_MARKUP && wxOSX_USE_COCOA

void wxAnyButton::OnEnterWindow( wxMouseEvent& WXUNUSED(event))
{
    if ( DoGetBitmap( State_Current ).IsOk() )
        GetPeer()->SetBitmap( DoGetBitmap( State_Current ) );
}

void wxAnyButton::OnLeaveWindow( wxMouseEvent& WXUNUSED(event))
{
    if ( DoGetBitmap( State_Current ).IsOk() )
        GetPeer()->SetBitmap( DoGetBitmap( State_Normal ) );
}
