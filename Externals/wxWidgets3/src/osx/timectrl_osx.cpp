///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/timectrl_osx.cpp
// Purpose:     Implementation of wxTimePickerCtrl for OS X.
// Author:      Vadim Zeitlin
// Created:     2011-12-18
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_TIMEPICKCTRL && wxOSX_USE_COCOA

#include "wx/timectrl.h"
#include "wx/dateevt.h"

#include "wx/osx/core/private/datetimectrl.h"

// ============================================================================
// wxTimePickerCtrl implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxTimePickerCtrl, wxControl);

bool
wxTimePickerCtrl::Create(wxWindow *parent,
                         wxWindowID id,
                         const wxDateTime& dt,
                         const wxPoint& pos,
                         const wxSize& size,
                         long style,
                         const wxValidator& validator,
                         const wxString& name)
{
    DontCreatePeer();

    if ( !wxTimePickerCtrlBase::Create(parent, id, pos, size,
                                       style, validator, name) )
        return false;

    wxOSXWidgetImpl* const peer = wxDateTimeWidgetImpl::CreateDateTimePicker
                                  (
                                    this,
                                    dt,
                                    pos,
                                    size,
                                    style,
                                    wxDateTimeWidget_HourMinuteSecond
                                  );
    if ( !peer )
        return false;

    SetPeer(peer);

    MacPostControlCreate(pos, size);

    return true;
}

void wxTimePickerCtrl::OSXGenerateEvent(const wxDateTime& dt)
{
    wxDateEvent event(this, dt, wxEVT_TIME_CHANGED);
    HandleWindowEvent(event);
}

#endif // wxUSE_TIMEPICKCTRL && wxOSX_USE_COCOA
