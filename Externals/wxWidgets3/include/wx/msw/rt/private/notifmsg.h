/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/rt/private/notifmsg.h
// Purpose:     WinRT implementation of wxNotificationMessageImpl
// Author:      Tobias Taschner
// Created:     2015-09-13
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_MSW_RT_PRIVATE_NOTIFMSG_H_
#define _WX_MSW_RT_PRIVATE_NOTIFMSG_H_

#include "wx/notifmsg.h"
#include "wx/private/notifmsg.h"

class wxToastNotificationHelper
{
public:
    static bool UseToasts(const wxString& shortcutPath,
        const wxString& appId);

    static bool IsEnabled();

    static wxNotificationMessageImpl* CreateInstance(wxNotificationMessageBase* notification);
};

#endif // _WX_MSW_RT_PRIVATE_NOTIFMSG_H_
