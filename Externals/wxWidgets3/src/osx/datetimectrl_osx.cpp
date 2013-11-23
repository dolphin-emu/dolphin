///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/datetimectrl_osx.cpp
// Purpose:     Implementation of wxDateTimePickerCtrl for OS X.
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

#if wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include "wx/datectrl.h"
#include "wx/datetimectrl.h"

#include "wx/osx/core/private/datetimectrl.h"

// ============================================================================
// wxDateTimePickerCtrl implementation
// ============================================================================

wxDateTimeWidgetImpl* wxDateTimePickerCtrl::GetDateTimePeer() const
{
    return static_cast<wxDateTimeWidgetImpl*>(GetPeer());
}

void wxDateTimePickerCtrl::SetValue(const wxDateTime& dt)
{
    if ( dt.IsValid() )
    {
        GetDateTimePeer()->SetDateTime(dt);
    }
    else // invalid date
    {
        wxASSERT_MSG( HasFlag(wxDP_ALLOWNONE),
                     wxT("this control must have a valid date") );

        // TODO setting to an invalid date is not natively supported
        // so we must implement a UI for that ourselves
        GetDateTimePeer()->SetDateTime(dt);
    }
}

wxDateTime wxDateTimePickerCtrl::GetValue() const
{
    return GetDateTimePeer()->GetDateTime();
}


#endif // wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL
