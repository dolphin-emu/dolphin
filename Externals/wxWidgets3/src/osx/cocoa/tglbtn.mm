/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/tglbtn.mm
// Purpose:     Definition of the wxToggleButton class, which implements a
//              toggle button under wxMac.
// Author:      Stefan Csomor
// Modified by:
// Created:     08.02.01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declatations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if wxUSE_TOGGLEBTN

#include "wx/tglbtn.h"
#include "wx/osx/private.h"

// from button.mm

extern "C" void SetBezelStyleFromBorderFlags(NSButton *v,
                                             long style,
                                             wxWindowID winid = wxID_ANY,
                                             const wxString& label = wxString(),
                                             const wxBitmap& bitmap = wxBitmap());

wxWidgetImplType* wxWidgetImpl::CreateToggleButton( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID winid,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSButton* v = [[wxNSButton alloc] initWithFrame:r];

    SetBezelStyleFromBorderFlags(v, style, winid, label);

    [v setButtonType:NSOnOffButton];
    wxWidgetCocoaImpl* c = new wxButtonCocoaImpl( wxpeer, v );
    return c;
}

wxWidgetImplType* wxWidgetImpl::CreateBitmapToggleButton( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID winid,
                                    const wxBitmap& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSButton* v = [[wxNSButton alloc] initWithFrame:r];

    SetBezelStyleFromBorderFlags(v, style, winid, wxString(), label);
    
    if (label.IsOk())
        [v setImage:label.GetNSImage() ];

    [v setButtonType:NSOnOffButton];
    wxWidgetCocoaImpl* c = new wxButtonCocoaImpl( wxpeer, v );
    return c;
}

#endif // wxUSE_TOGGLEBTN

