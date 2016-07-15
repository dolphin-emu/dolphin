///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/notifmsg.cpp
// Purpose:     implementation of wxNotificationMessage for Windows
// Author:      Vadim Zeitlin
// Created:     2007-12-01
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
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

// we can only use the native implementation if we have a working
// wxTaskBarIcon::ShowBalloon() method
#if wxUSE_NOTIFICATION_MESSAGE && \
        wxUSE_TASKBARICON && wxUSE_TASKBARICON_BALLOONS

#include "wx/notifmsg.h"

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
    #include "wx/app.h"
    #include "wx/string.h"
    #include  "wx/app.h"
#endif // WX_PRECOMP

#include "wx/private/notifmsg.h"
#include "wx/msw/rt/private/notifmsg.h"

#include "wx/taskbar.h"

// ----------------------------------------------------------------------------
// different implementations used by wxNotificationMessage
// ----------------------------------------------------------------------------

// implementations using a taskbar icon and balloons
class wxBalloonNotifMsgImpl : public wxNotificationMessageImpl
{
public:
    // Ctor creates the associated taskbar icon (using the icon of the top
    // level parent of the given window) unless UseTaskBarIcon() had been
    // previously called  which can be used to show an attached balloon later
    // by the derived classes.
    wxBalloonNotifMsgImpl(wxNotificationMessageBase* notification) :
        wxNotificationMessageImpl(notification),
        m_flags(wxICON_INFORMATION),
        m_parent(NULL)
    {

    }

    virtual ~wxBalloonNotifMsgImpl();

    virtual bool Show(int timeout) wxOVERRIDE;

    virtual bool Close() wxOVERRIDE;

    virtual void SetTitle(const wxString& title) wxOVERRIDE
    {
        m_title = title;
    }

    virtual void SetMessage(const wxString& message) wxOVERRIDE
    {
        m_message = message;
    }

    virtual void SetParent(wxWindow *parent) wxOVERRIDE
    {
        m_parent = parent;
    }

    virtual void SetFlags(int flags) wxOVERRIDE
    {
        m_flags = flags;
    }

    virtual void SetIcon(const wxIcon& icon) wxOVERRIDE
    {
        m_icon = icon;
    }

    virtual bool AddAction(wxWindowID WXUNUSED(actionid), const wxString &WXUNUSED(label))
    {
        // Actions are not supported in balloon notifications
        return false;
    }

    // implementation of wxNotificationMessage method with the same name
    static wxTaskBarIcon *UseTaskBarIcon(wxTaskBarIcon *icon);

    // Returns true if we're using our own icon or false if we're hitching a
    // ride on the application icon provided to us via UseTaskBarIcon().
    static bool IsUsingOwnIcon()
    {
        return ms_refCountIcon != -1;
    }

    // Indicates that the taskbar icon we're using has been hidden and can be
    // deleted.
    //
    // This is only called by wxNotificationIconEvtHandler and should only be
    // called when using our own icon (as opposed to the one passed to us via
    // UseTaskBarIcon()).
    static void ReleaseIcon()
    {
        wxASSERT_MSG( ms_refCountIcon != -1,
                      wxS("Must not be called when not using own icon") );

        if ( ms_refCountIcon > 0 && !--ms_refCountIcon )
        {
            delete ms_icon;
            ms_icon = NULL;
        }
    }

protected:
    // Creates a new icon if necessary, see the comment below.
    void SetUpIcon(wxWindow *win);


    // We need an icon to show the notification in a balloon attached to it.
    // It may happen that the main application already shows an icon in the
    // taskbar notification area in which case it should call our
    // UseTaskBarIcon() and we just use this icon without ever allocating nor
    // deleting it and ms_refCountIcon is -1 and never changes. Otherwise, we
    // create the icon when we need it the first time but reuse it if we need
    // to show subsequent notifications while this icon is still alive. This is
    // needed in order to avoid 2 or 3 or even more identical icons if a couple
    // of notifications are shown in a row (which happens quite easily in
    // practice because Windows helpfully buffers all the notifications that
    // were generated while the user was away -- i.e. the screensaver was
    // active -- and then shows them all at once when the user comes back). In
    // this case, ms_refCountIcon is used as a normal reference counter, i.e.
    // the icon is only destroyed when it reaches 0.
    static wxTaskBarIcon *ms_icon;
    static int ms_refCountIcon;
private:
    wxString m_title;
    wxString m_message;
    int m_flags;
    wxIcon m_icon;
    wxWindow* m_parent;

    void OnTimeout(wxTaskBarIconEvent& event);
    void OnClick(wxTaskBarIconEvent& event);

    void OnIconHidden();
};

// ----------------------------------------------------------------------------

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxBalloonNotifMsgImpl
// ----------------------------------------------------------------------------

wxTaskBarIcon *wxBalloonNotifMsgImpl::ms_icon = NULL;
int wxBalloonNotifMsgImpl::ms_refCountIcon = 0;

/* static */
wxTaskBarIcon *wxBalloonNotifMsgImpl::UseTaskBarIcon(wxTaskBarIcon *icon)
{
    wxTaskBarIcon * const iconOld = ms_icon;
    ms_icon = icon;

    // Don't use reference counting for the provided icon, we don't own it.
    ms_refCountIcon = icon ? -1 : 0;

    return iconOld;
}

wxBalloonNotifMsgImpl::~wxBalloonNotifMsgImpl()
{
}

void wxBalloonNotifMsgImpl::OnIconHidden()
{
    SetActive(false);
    if ( ms_icon )
    {
        ms_icon->Unbind(wxEVT_TASKBAR_BALLOON_CLICK, &wxBalloonNotifMsgImpl::OnClick, this);
        ms_icon->Unbind(wxEVT_TASKBAR_BALLOON_TIMEOUT, &wxBalloonNotifMsgImpl::OnTimeout, this);
    }

    if ( IsUsingOwnIcon() )
        wxBalloonNotifMsgImpl::ReleaseIcon();
}

void wxBalloonNotifMsgImpl::OnTimeout(wxTaskBarIconEvent& WXUNUSED(event))
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_DISMISSED);
    ProcessNotificationEvent(evt);

    OnIconHidden();
}

void wxBalloonNotifMsgImpl::OnClick(wxTaskBarIconEvent& WXUNUSED(event))
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_CLICK);
    ProcessNotificationEvent(evt);

    OnIconHidden();
}

void wxBalloonNotifMsgImpl::SetUpIcon(wxWindow *win)
{
    if ( ms_icon )
    {
        // Increment the reference count if we manage the icon on our own.
        if ( ms_refCountIcon != -1 )
            ms_refCountIcon++;
    }
    else // Create a new icon.
    {
        wxASSERT_MSG( ms_refCountIcon == 0,
                      wxS("Shouldn't reference not existent icon") );

        ms_icon = new wxTaskBarIcon;
        ms_refCountIcon = 1;

        // use the icon of the associated (or main, if none) frame
        wxIcon icon;
        if ( win )
            win = wxGetTopLevelParent(win);
        if ( !win )
            win = wxTheApp->GetTopWindow();
        if ( win )
        {
            const wxTopLevelWindow * const
                tlw = wxDynamicCast(win, wxTopLevelWindow);
            if ( tlw )
                icon = tlw->GetIcon();
        }

        if ( !icon.IsOk() )
        {
            // we really must have some icon
            icon = wxIcon(wxT("wxICON_AAA"));
        }

        ms_icon->SetIcon(icon);
    }
}

bool
wxBalloonNotifMsgImpl::Show(int timeout)
{
    // timout active event
    wxTaskBarIconEvent event(wxEVT_TASKBAR_BALLOON_TIMEOUT, ms_icon);
    OnTimeout(event);

    SetUpIcon(m_parent);

    if ( !ms_icon->IsIconInstalled() )
    {
        // If we failed to install the icon (which does happen sometimes,
        // although only in unusual circumstances, e.g. it happens regularly,
        // albeit not constantly, if we're used soon after resume from suspend
        // under Windows 7), we should not call ShowBalloon() because it would
        // just assert and return and we must delete the icon ourselves because
        // otherwise its associated wxTaskBarIconWindow would remain alive
        // forever because we're not going to receive a notification about icon
        // disappearance from the system if we failed to install it in the
        // first place.
        delete ms_icon;
        ms_icon = NULL;
        ms_refCountIcon = 0;

        return false;
    }

    // Since Windows Vista timeout is ignored so this values are only for XP
    if ( timeout == wxNotificationMessage::Timeout_Auto )
    {
        // choose a value more or less in the middle of the allowed range
        timeout = 1;
    }
    else if ( timeout == wxNotificationMessage::Timeout_Never )
    {
        // use maximal (in Windows XP) timeout (but it will still
        // disappear on its own)
        timeout = 30;
    }

    timeout *= 1000; // Windows expresses timeout in milliseconds

    bool res = ms_icon->ShowBalloon(m_title, m_message, timeout, m_flags, m_icon);
    if ( res )
    {
        ms_icon->Bind(wxEVT_TASKBAR_BALLOON_CLICK, &wxBalloonNotifMsgImpl::OnClick, this);
        ms_icon->Bind(wxEVT_TASKBAR_BALLOON_TIMEOUT, &wxBalloonNotifMsgImpl::OnTimeout, this);
        SetActive(true);
    }

    return res;
}

bool wxBalloonNotifMsgImpl::Close()
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_DISMISSED);
    ProcessNotificationEvent(evt);

    OnIconHidden();

    if ( !IsUsingOwnIcon() && ms_icon )
    {
        // just hide the balloon
        ms_icon->ShowBalloon("", "");
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxNotificationMessage
// ----------------------------------------------------------------------------

/* static */
wxTaskBarIcon *wxNotificationMessage::UseTaskBarIcon(wxTaskBarIcon *icon)
{
    return wxBalloonNotifMsgImpl::UseTaskBarIcon(icon);
}

bool wxNotificationMessage::MSWUseToasts(
    const wxString& shortcutPath,
    const wxString& appId)
{
    return wxToastNotificationHelper::UseToasts(shortcutPath, appId);
}

void wxNotificationMessage::Init()
{
    if ( wxToastNotificationHelper::IsEnabled() )
        m_impl = wxToastNotificationHelper::CreateInstance(this);
    else
        m_impl = new wxBalloonNotifMsgImpl(this);
}

#endif // wxUSE_NOTIFICATION_MESSAGE && wxUSE_TASKBARICON
