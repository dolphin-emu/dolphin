///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/notifmsg.cpp
// Purpose:     wxNotificationMessage for wxGTK using libnotify.
// Author:      Vadim Zeitlin
// Created:     2012-07-25
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_NOTIFICATION_MESSAGE && wxUSE_LIBNOTIFY

#include "wx/notifmsg.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif // WX_PRECOMP

#include <libnotify/notify.h>

#include "wx/module.h"

// General note about error handling: as notifications are meant to be
// non-intrusive, we use wxLogDebug() and not wxLogError() if anything goes
// wrong here to avoid spamming the user with message boxes. As all methods
// return boolean indicating success or failure, the caller could show the
// notification in some other way or notify about the error itself if needed.
#include "wx/gtk/private/error.h"

// ----------------------------------------------------------------------------
// A module for cleaning up libnotify on exit.
// ----------------------------------------------------------------------------

class wxLibnotifyModule : public wxModule
{
public:
    virtual bool OnInit() wxOVERRIDE
    {
        // We're initialized on demand.
        return true;
    }

    virtual void OnExit() wxOVERRIDE
    {
        if ( notify_is_initted() )
            notify_uninit();
    }

    // Do initialize the library.
    static bool Initialize()
    {
        if ( !notify_is_initted() )
        {
            if ( !notify_init(wxTheApp->GetAppName().utf8_str()) )
                return false;
        }

        return true;
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxLibnotifyModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxLibnotifyModule, wxModule);

// ============================================================================
// wxNotificationMessage implementation
// ============================================================================

bool wxNotificationMessage::GTKSetIconName(const wxString& name)
{
    m_iconName = name;

    return true;
}

bool wxNotificationMessage::Show(int timeout)
{
    if ( !wxLibnotifyModule::Initialize() )
        return false;

    // Determine the GTK+ icon to use from flags and also set the urgency
    // appropriately.
    const char* icon;
    NotifyUrgency urgency;
    switch ( GetFlags() )
    {
        case wxICON_INFORMATION:
            icon = "dialog-information";
            urgency = NOTIFY_URGENCY_LOW;
            break;

        case wxICON_WARNING:
            icon = "dialog-warning";
            urgency = NOTIFY_URGENCY_NORMAL;
            break;

        case wxICON_ERROR:
            icon = "dialog-error";
            urgency = NOTIFY_URGENCY_CRITICAL;
            break;

        default:
            wxFAIL_MSG( "Unknown notification message flags." );
            return false;
    }

    // Explicitly specified icon name overrides the implicit one determined by
    // the flags.
    wxScopedCharBuffer buf;
    if ( !m_iconName.empty() )
    {
        buf = m_iconName.utf8_str();
        icon = buf;
    }

    // Create the notification or update an existing one if we had already been
    // shown before.
    if ( !m_notification )
    {
        m_notification = notify_notification_new
                         (
                            GetTitle().utf8_str(),
                            GetMessage().utf8_str(),
                            icon
#if !wxUSE_LIBNOTIFY_0_7
                            // There used to be an "associated window"
                            // parameter in this function but it has
                            // disappeared by 0.7, so use it for previous
                            // versions only.
                            , 0
#endif // libnotify < 0.7
                         );
        if ( !m_notification )
        {
            wxLogDebug("Failed to creation notification.");

            return false;
        }
    }
    else
    {
        if ( !notify_notification_update
              (
                m_notification,
                GetTitle().utf8_str(),
                GetMessage().utf8_str(),
                icon
              ) )
        {
            wxLogDebug(wxS("notify_notification_update() unexpectedly failed."));
        }
    }


    // Set the notification parameters not specified during creation.
    notify_notification_set_timeout
    (
        m_notification,
        timeout == Timeout_Auto ? NOTIFY_EXPIRES_DEFAULT
                                : timeout == Timeout_Never ? NOTIFY_EXPIRES_NEVER
                                                           : 1000*timeout
    );

    notify_notification_set_urgency(m_notification, urgency);


    // Finally do show the notification.
    wxGtkError error;
    if ( !notify_notification_show(m_notification, error.Out()) )
    {
        wxLogDebug("Failed to shown notification: %s", error.GetMessage());

        return false;
    }

    return true;
}

bool wxNotificationMessage::Close()
{
    wxCHECK_MSG( m_notification, false,
                 wxS("Can't close not shown notification.") );

    wxGtkError error;
    if ( !notify_notification_close(m_notification, error.Out()) )
    {
        wxLogDebug("Failed to hide notification: %s", error.GetMessage());

        return false;
    }

    return true;
}

wxNotificationMessage::~wxNotificationMessage()
{
    if ( m_notification )
        g_object_unref(m_notification);
}

#endif // wxUSE_NOTIFICATION_MESSAGE && wxUSE_LIBNOTIFY
