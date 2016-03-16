///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/activityindicator.cpp
// Purpose:     wxActivityIndicator implementation for wxOSX/Cocoa.
// Author:      Vadim Zeitlin
// Created:     2015-03-08
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ACTIVITYINDICATOR

#include "wx/activityindicator.h"

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// local helpers
// ----------------------------------------------------------------------------

namespace
{

// Return the NSProgressIndicator from the given peer.
inline
NSProgressIndicator *GetProgressIndicator(wxWidgetImpl* peer)
{
    return peer ? static_cast<NSProgressIndicator*>(peer->GetWXWidget())
                : NULL;
}

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxActivityIndicator, wxControl);

bool
wxActivityIndicator::Create(wxWindow* parent,
                            wxWindowID winid,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxString& name)
{
    DontCreatePeer();

    // Notice that we skip wxControl version, we don't need the validator
    // support that it adds.
    if ( !wxWindow::Create(parent, winid, pos, size, style, name) )
        return false;

    NSRect r = wxOSXGetFrameForControl(this, pos , size);
    NSProgressIndicator* w = [[NSProgressIndicator alloc] initWithFrame:r];
    [w setStyle: NSProgressIndicatorSpinningStyle];

    SetPeer(new wxWidgetCocoaImpl(this, w));

    MacPostControlCreate(pos, size);

    return true;
}

void wxActivityIndicator::Start()
{
    if ( m_isRunning )
        return;

    NSProgressIndicator* const ind = GetProgressIndicator(GetPeer());
    wxCHECK_RET( ind, wxS("Must be created first") );

    [ind startAnimation:nil];

    m_isRunning = true;
}

void wxActivityIndicator::Stop()
{
    if ( !m_isRunning )
        return;

    NSProgressIndicator* const ind = GetProgressIndicator(GetPeer());
    wxCHECK_RET( ind, wxS("Must be created first") );

    [ind stopAnimation:nil];

    m_isRunning = false;
}

bool wxActivityIndicator::IsRunning() const
{
    return m_isRunning;
}

#endif // wxUSE_ACTIVITYINDICATOR
