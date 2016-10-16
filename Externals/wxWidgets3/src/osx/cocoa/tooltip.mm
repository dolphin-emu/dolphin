/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/tooltip.mm
// Purpose:     wxToolTip implementation
// Author:      Stefan Csomor
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TOOLTIPS

#include "wx/tooltip.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/window.h"
    #include "wx/dc.h"
    #include "wx/timer.h"
    #include "wx/nonownedwnd.h"
#endif // WX_PRECOMP

#include "wx/geometry.h"
#include "wx/osx/uma.h"

#if wxOSX_USE_COCOA_OR_CARBON
    #include <AppKit/AppKit.h>
#endif

// FYI a link to help with implementing: http://www.cocoadev.com/index.pl?LittleYellowBox


//-----------------------------------------------------------------------------
// wxToolTip
//-----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxToolTip, wxObject);


wxToolTip::wxToolTip( const wxString &tip )
{
    m_text = tip;
    m_window = NULL;
}

wxToolTip::~wxToolTip()
{
}

void wxToolTip::SetTip( const wxString &tip )
{
    m_text = tip;
    if (m_window)
        m_window->SetToolTip(this);
}

void wxToolTip::SetWindow( wxWindow *win )
{
    m_window = win ;
}

void wxToolTip::Enable( bool WXUNUSED(flag) )
{
}

void wxToolTip::SetDelay( long msecs )
{
#if wxOSX_USE_COCOA_OR_CARBON
    [[NSUserDefaults standardUserDefaults] setObject: [NSNumber numberWithInt: msecs]
                                              forKey: @"NSInitialToolTipDelay"];
#endif
}

void wxToolTip::SetAutoPop( long WXUNUSED(msecs) )
{
}

void wxToolTip::SetReshow( long WXUNUSED(msecs) )
{
}

void wxToolTip::RelayEvent( wxWindow *WXUNUSED(win) , wxMouseEvent &WXUNUSED(event) )
{
}

void wxToolTip::RemoveToolTips()
{
}

// --- mac specific
void wxToolTip::NotifyWindowDelete( WXHWND WXUNUSED(win) )
{
}

#endif // wxUSE_TOOLTIPS
