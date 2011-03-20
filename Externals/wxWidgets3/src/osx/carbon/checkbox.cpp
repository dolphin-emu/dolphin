/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/checkbox.cpp
// Purpose:     wxCheckBox
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: checkbox.cpp 61724 2009-08-21 10:41:26Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHECKBOX

#include "wx/checkbox.h"
#include "wx/osx/uma.h"

wxWidgetImplType* wxWidgetImpl::CreateCheckBox( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* peer = new wxMacControl(wxpeer) ;

    verify_noerr( CreateCheckBoxControl(MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds ,
        CFSTR("") , 0 , false , peer->GetControlRefAddr() ) );
    SInt32 maxValue = 1 /* kControlCheckboxCheckedValue */;
    if (style & wxCHK_3STATE)
        maxValue = 2 /* kControlCheckboxMixedValue */;

    peer->SetMaximum( maxValue ) ;

    return peer;
}

#endif
