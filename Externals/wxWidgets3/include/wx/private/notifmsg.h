///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/notifmsg.h
// Purpose:     wxNotificationMessage declarations
// Author:      Tobias Taschner
// Created:     2015-08-04
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_NOTIFMSG_H_
#define _WX_PRIVATE_NOTIFMSG_H_

class wxNotificationMessageImpl
{
public:
    wxNotificationMessageImpl(wxNotificationMessageBase* notification):
        m_notification(notification),
        m_active(false)
    {

    }

    virtual ~wxNotificationMessageImpl() { }

    virtual bool Show(int timeout) = 0;

    virtual bool Close() = 0;

    virtual void SetTitle(const wxString& title) = 0;

    virtual void SetMessage(const wxString& message) = 0;

    virtual void SetParent(wxWindow *parent) = 0;

    virtual void SetFlags(int flags) = 0;

    virtual void SetIcon(const wxIcon& icon) = 0;

    virtual bool AddAction(wxWindowID actionid, const wxString &label) = 0;

    virtual void Detach()
    {
        if (m_active)
            m_notification = NULL;
        else
            delete this;
    }

    bool ProcessNotificationEvent(wxEvent& event)
    {
        if (m_notification)
            return m_notification->ProcessEvent(event);
        else
            return false;
    }

    wxNotificationMessageBase* GetNotification() const
    {
        return m_notification;
    }

protected:
    wxNotificationMessageBase* m_notification;
    bool m_active;

    void SetActive(bool active)
    {
        m_active = active;

        // Delete the implementation if the notification is detached
        if (!m_notification && !active)
            delete this;
    }
};

#endif // _WX_PRIVATE_NOTIFMSG_H_
