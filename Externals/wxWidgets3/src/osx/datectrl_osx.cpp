///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/datectrl_osx.cpp
// Purpose:     Implementation of wxDatePickerCtrl for OS X.
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

#if wxUSE_DATEPICKCTRL && wxOSX_USE_COCOA

#include "wx/datectrl.h"
#include "wx/dateevt.h"

#include "wx/osx/core/private/datetimectrl.h"

// ============================================================================
// wxDatePickerCtrl implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxDatePickerCtrl, wxControl);

bool
wxDatePickerCtrl::Create(wxWindow *parent,
                         wxWindowID id,
                         const wxDateTime& dt,
                         const wxPoint& pos,
                         const wxSize& size,
                         long style,
                         const wxValidator& validator,
                         const wxString& name)
{
    DontCreatePeer();

    if ( !wxDatePickerCtrlBase::Create(parent, id, pos, size,
                                       style, validator, name) )
        return false;

    wxOSXWidgetImpl* const peer = wxDateTimeWidgetImpl::CreateDateTimePicker
                                  (
                                    this,
                                    dt,
                                    pos,
                                    size,
                                    style,
                                    wxDateTimeWidget_YearMonthDay
                                  );
    if ( !peer )
        return false;

    SetPeer(peer);

    MacPostControlCreate(pos, size);

    return true;
}

void wxDatePickerCtrl::SetRange(const wxDateTime& dt1, const wxDateTime& dt2)
{
    GetDateTimePeer()->SetDateRange(dt1, dt2);
}

bool wxDatePickerCtrl::GetRange(wxDateTime *dt1, wxDateTime *dt2) const
{
    return GetDateTimePeer()->GetDateRange(dt1, dt2);
}

void wxDatePickerCtrl::OSXGenerateEvent(const wxDateTime& dt)
{
    wxDateEvent event(this, dt, wxEVT_DATE_CHANGED);
    HandleWindowEvent(event);
}

#endif // wxUSE_DATEPICKCTRL && wxOSX_USE_COCOA
