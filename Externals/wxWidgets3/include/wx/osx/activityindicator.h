///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/activityindicator.h
// Purpose:     Declaration of wxActivityIndicator for wxOSX (Cocoa only).
// Author:      Vadim Zeitlin
// Created:     2015-03-08
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_ACTIVITYINDICATOR_H_
#define _WX_OSX_ACTIVITYINDICATOR_H_

// ----------------------------------------------------------------------------
// wxActivityIndicator: implementation using GtkSpinner.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxActivityIndicator : public wxActivityIndicatorBase
{
public:
    wxActivityIndicator()
    {
        Init();
    }

    explicit
    wxActivityIndicator(wxWindow* parent,
                        wxWindowID winid = wxID_ANY,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = 0,
                        const wxString& name = wxActivityIndicatorNameStr)
    {
        Init();

        Create(parent, winid, pos, size, style, name);
    }

    bool Create(wxWindow* parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxActivityIndicatorNameStr);

    virtual void Start() wxOVERRIDE;
    virtual void Stop() wxOVERRIDE;
    virtual bool IsRunning() const wxOVERRIDE;

private:
    // Common part of all ctors.
    void Init() { m_isRunning = false; }

    bool m_isRunning;

    wxDECLARE_DYNAMIC_CLASS(wxActivityIndicator);
    wxDECLARE_NO_COPY_CLASS(wxActivityIndicator);
};

#endif // _WX_OSX_ACTIVITYINDICATOR_H_
