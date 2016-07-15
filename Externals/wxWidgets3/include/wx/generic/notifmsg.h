///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/notifmsg.h
// Purpose:     generic implementation of wxGenericNotificationMessage
// Author:      Vadim Zeitlin
// Created:     2007-11-24
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_NOTIFMSG_H_
#define _WX_GENERIC_NOTIFMSG_H_

// ----------------------------------------------------------------------------
// wxGenericNotificationMessage
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGenericNotificationMessage : public wxNotificationMessageBase
{
public:
    wxGenericNotificationMessage()
    {
        Init();
    }

    wxGenericNotificationMessage(const wxString& title,
                                 const wxString& message = wxString(),
                                 wxWindow *parent = NULL,
                                 int flags = wxICON_INFORMATION)
    {
        Init();
        Create(title, message, parent, flags);
    }

    // generic implementation-specific methods

    // get/set the default timeout (used if Timeout_Auto is specified)
    static int GetDefaultTimeout();
    static void SetDefaultTimeout(int timeout);

private:
    void Init();

    wxDECLARE_NO_COPY_CLASS(wxGenericNotificationMessage);
};

#endif // _WX_GENERIC_NOTIFMSG_H_

