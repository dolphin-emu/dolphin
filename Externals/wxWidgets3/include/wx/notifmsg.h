///////////////////////////////////////////////////////////////////////////////
// Name:        wx/notifmsg.h
// Purpose:     class allowing to show notification messages to the user
// Author:      Vadim Zeitlin
// Created:     2007-11-19
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NOTIFMSG_H_
#define _WX_NOTIFMSG_H_

#include "wx/event.h"

#if wxUSE_NOTIFICATION_MESSAGE

// ----------------------------------------------------------------------------
// wxNotificationMessage: allows to show the user a message non intrusively
// ----------------------------------------------------------------------------

// notice that this class is not a window and so doesn't derive from wxWindow

class WXDLLIMPEXP_ADV wxNotificationMessageBase : public wxEvtHandler
{
public:
    // ctors and initializers
    // ----------------------

    // default ctor, use setters below to initialize it later
    wxNotificationMessageBase()
    {
        Init();
    }

    // create a notification object with the given title and message (the
    // latter may be empty in which case only the title will be shown)
    wxNotificationMessageBase(const wxString& title,
                              const wxString& message = wxEmptyString,
                              wxWindow *parent = NULL,
                              int flags = wxICON_INFORMATION)
    {
        Init();
        Create(title, message, parent, flags);
    }

    virtual ~wxNotificationMessageBase();

    // note that the setters must be called before Show()

    // set the title: short string, markup not allowed
    void SetTitle(const wxString& title);

    // set the text of the message: this is a longer string than the title and
    // some platforms allow simple HTML-like markup in it
    void SetMessage(const wxString& message);

    // set the parent for this notification: we'll be associated with the top
    // level parent of this window or, if this method is not called, with the
    // main application window by default
    void SetParent(wxWindow *parent);

    // this method can currently be used to choose a standard icon to use: the
    // parameter may be one of wxICON_INFORMATION, wxICON_WARNING or
    // wxICON_ERROR only (but not wxICON_QUESTION)
    void SetFlags(int flags);

    // set a custom icon to use instead of the system provided specified via SetFlags
    virtual void SetIcon(const wxIcon& icon);

    // Add a button to the notification, returns false if the platform does not support
    // actions in notifications
    virtual bool AddAction(wxWindowID actionid, const wxString &label = wxString());

    // showing and hiding
    // ------------------

    // possible values for Show() timeout
    enum
    {
        Timeout_Auto = -1,  // notification will be hidden automatically
        Timeout_Never = 0   // notification will never time out
    };

    // show the notification to the user and hides it after timeout seconds
    // pass (special values Timeout_Auto and Timeout_Never can be used)
    //
    // returns false if an error occurred
    bool Show(int timeout = Timeout_Auto);

    // hide the notification, returns true if it was hidden or false if it
    // couldn't be done (e.g. on some systems automatically hidden
    // notifications can't be hidden manually)
    bool Close();

protected:
    // Common part of all ctors.
    void Create(const wxString& title = wxEmptyString,
        const wxString& message = wxEmptyString,
        wxWindow *parent = NULL,
        int flags = wxICON_INFORMATION)
    {
        SetTitle(title);
        SetMessage(message);
        SetParent(parent);
        SetFlags(flags);
    }

    class wxNotificationMessageImpl* m_impl;

private:

    void Init()
    {
        m_impl = NULL;
    }

    wxDECLARE_NO_COPY_CLASS(wxNotificationMessageBase);
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_NOTIFICATION_MESSAGE_CLICK, wxCommandEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_NOTIFICATION_MESSAGE_DISMISSED, wxCommandEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_NOTIFICATION_MESSAGE_ACTION, wxCommandEvent );

#if (defined(__WXGTK__) && wxUSE_LIBNOTIFY) || \
    (defined(__WXMSW__) && wxUSE_TASKBARICON && wxUSE_TASKBARICON_BALLOONS) || \
    (defined(__WXOSX_COCOA__) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_8))
    #define wxHAS_NATIVE_NOTIFICATION_MESSAGE
#endif

// ----------------------------------------------------------------------------
// wxNotificationMessage
// ----------------------------------------------------------------------------

#ifdef wxHAS_NATIVE_NOTIFICATION_MESSAGE

#if defined(__WXMSW__)
class WXDLLIMPEXP_FWD_ADV wxTaskBarIcon;
#endif // defined(__WXMSW__)

#else
#include "wx/generic/notifmsg.h"
#endif // wxHAS_NATIVE_NOTIFICATION_MESSAGE

class WXDLLIMPEXP_ADV wxNotificationMessage : public
#ifdef wxHAS_NATIVE_NOTIFICATION_MESSAGE
    wxNotificationMessageBase
#else
    wxGenericNotificationMessage
#endif
{
public:
    wxNotificationMessage() { Init(); }
    wxNotificationMessage(const wxString& title,
                          const wxString& message = wxString(),
                          wxWindow *parent = NULL,
                          int flags = wxICON_INFORMATION)
    {
        Init();
        Create(title, message, parent, flags);
    }

#if defined(__WXMSW__) && defined(wxHAS_NATIVE_NOTIFICATION_MESSAGE)
    static bool MSWUseToasts(
        const wxString& shortcutPath = wxString(),
        const wxString& appId = wxString());

    // returns the task bar icon which was used previously (may be NULL)
    static wxTaskBarIcon *UseTaskBarIcon(wxTaskBarIcon *icon);

#endif // defined(__WXMSW__) && defined(wxHAS_NATIVE_NOTIFICATION_MESSAGE)

private:
    // common part of all ctors
    void Init();

    wxDECLARE_NO_COPY_CLASS(wxNotificationMessage);
};

#endif // wxUSE_NOTIFICATION_MESSAGE

#endif // _WX_NOTIFMSG_H_

