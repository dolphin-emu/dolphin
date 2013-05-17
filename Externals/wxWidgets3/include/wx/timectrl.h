///////////////////////////////////////////////////////////////////////////////
// Name:        wx/timectrl.h
// Purpose:     Declaration of wxTimePickerCtrl class.
// Author:      Vadim Zeitlin
// Created:     2011-09-22
// RCS-ID:      $Id: timectrl.h 70071 2011-12-20 21:27:14Z VZ $
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TIMECTRL_H_
#define _WX_TIMECTRL_H_

#include "wx/defs.h"

#if wxUSE_TIMEPICKCTRL

#include "wx/datetimectrl.h"

#define wxTimePickerCtrlNameStr wxS("timectrl")

// No special styles are currently defined for this control but still define a
// symbolic constant for the default style for consistency.
enum
{
    wxTP_DEFAULT = 0
};

// ----------------------------------------------------------------------------
// wxTimePickerCtrl: Allow the user to enter the time.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxTimePickerCtrlBase : public wxDateTimePickerCtrl
{
public:
    /*
        The derived classes should implement ctor and Create() method with the
        following signature:

        bool Create(wxWindow *parent,
                    wxWindowID id,
                    const wxDateTime& dt = wxDefaultDateTime,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxTP_DEFAULT,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxTimePickerCtrlNameStr);
     */

    /*
        We also inherit Set/GetValue() methods from the base class which define
        our public API. Notice that the date portion of the date passed as
        input is ignored and for the result date it's always today, but only
        the time part of wxDateTime objects is really significant here.
     */
};

#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    #include "wx/msw/timectrl.h"

    #define wxHAS_NATIVE_TIMEPICKERCTRL
#elif defined(__WXOSX_COCOA__) && !defined(__WXUNIVERSAL__)
    #include "wx/osx/timectrl.h"

    #define wxHAS_NATIVE_TIMEPICKERCTRL
#else
    #include "wx/generic/timectrl.h"

    class WXDLLIMPEXP_ADV wxTimePickerCtrl : public wxTimePickerCtrlGeneric
    {
    public:
        wxTimePickerCtrl() { }
        wxTimePickerCtrl(wxWindow *parent,
                         wxWindowID id,
                         const wxDateTime& date = wxDefaultDateTime,
                         const wxPoint& pos = wxDefaultPosition,
                         const wxSize& size = wxDefaultSize,
                         long style = wxTP_DEFAULT,
                         const wxValidator& validator = wxDefaultValidator,
                         const wxString& name = wxTimePickerCtrlNameStr)
            : wxTimePickerCtrlGeneric(parent, id, date, pos, size, style, validator, name)
        {
        }

    private:
        wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxTimePickerCtrl);
    };
#endif

#endif // wxUSE_TIMEPICKCTRL

#endif // _WX_TIMECTRL_H_
