///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/notifmsg.cpp
// Purpose:     implementation of wxNotificationMessage for Windows
// Author:      Vadim Zeitlin
// Created:     2007-12-01
// RCS-ID:      $Id: notifmsg.cpp 61508 2009-07-23 20:30:22Z VZ $
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
#endif // WX_PRECOMP

#include "wx/generic/notifmsg.h"

#include "wx/taskbar.h"

// ----------------------------------------------------------------------------
// different implementations used by wxNotificationMessage
// ----------------------------------------------------------------------------

// base class for all available implementations
class wxNotifMsgImpl
{
public:
    wxNotifMsgImpl() { }
    virtual ~wxNotifMsgImpl() { }

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags) = 0;
    virtual bool DoClose() = 0;

private:
    wxDECLARE_NO_COPY_CLASS(wxNotifMsgImpl);
};

// implementation which is simply a bridge to wxGenericNotificationMessage
class wxGenericNotifMsgImpl : public wxNotifMsgImpl
{
public:
    wxGenericNotifMsgImpl() : m_notif(new wxGenericNotificationMessage) { }
    virtual ~wxGenericNotifMsgImpl() { delete m_notif; }

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags)
    {
        m_notif->SetTitle(title);
        m_notif->SetMessage(message);
        m_notif->SetFlags(flags);
        return m_notif->Show(timeout);
    }

    virtual bool DoClose()
    {
        return m_notif->Close();
    }

private:
    wxGenericNotificationMessage * const m_notif;
};

// common base class for implementations using a taskbar icon and balloons
class wxBalloonNotifMsgImpl : public wxNotifMsgImpl
{
public:
    // ctor sets up m_icon (using the icon of the top level parent of the given
    // window) which can be used to show an attached balloon later by the
    // derived classes
    wxBalloonNotifMsgImpl(wxWindow *win) { SetUpIcon(win); }

    // implementation of wxNotificationMessage method with the same name
    static wxTaskBarIcon *UseTaskBarIcon(wxTaskBarIcon *icon);

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags);

protected:
    // sets up m_icon (doesn't do anything with the old value, caller beware)
    void SetUpIcon(wxWindow *win);


    static wxTaskBarIcon *ms_iconToUse;

    // the icon we attach our notification to, either ms_iconToUse or a
    // temporary one which we will destroy when done
    wxTaskBarIcon *m_icon;

    // should be only used if m_icon != NULL and indicates whether we should
    // delete it
    bool m_ownsIcon;
};

// implementation for automatically hidden notifications
class wxAutoNotifMsgImpl : public wxBalloonNotifMsgImpl
{
public:
    wxAutoNotifMsgImpl(wxWindow *win);

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags);

    // can't close automatic notification [currently]
    virtual bool DoClose() { return false; }

private:
    // custom event handler connected to m_icon which will receive the icon
    // close event and delete it and itself when it happens
    wxEvtHandler * const m_iconEvtHandler;
};

// implementation for manually closed notifications
class wxManualNotifMsgImpl : public wxBalloonNotifMsgImpl
{
public:
    wxManualNotifMsgImpl(wxWindow *win);
    virtual ~wxManualNotifMsgImpl();

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags);
    virtual bool DoClose();

private:
    // store ctor parameter as we need it to recreate the icon later if we're
    // closed and shown again
    wxWindow * const m_win;
};

// ----------------------------------------------------------------------------
// custom event handler for task bar icons
// ----------------------------------------------------------------------------

// normally we'd just use a custom taskbar icon class but this is impossible
// because we can be asked to attach the notifications to an existing icon
// which we didn't create, hence we install a special event handler allowing us
// to get the events we need (and, crucially, to delete the icon when it's not
// needed any more) in any case

class wxNotificationIconEvtHandler : public wxEvtHandler
{
public:
    wxNotificationIconEvtHandler(wxTaskBarIcon *icon);

private:
    void OnTimeout(wxTaskBarIconEvent& event);
    void OnClick(wxTaskBarIconEvent& event);

    void OnIconHidden();


    wxTaskBarIcon * const m_icon;

    wxDECLARE_NO_COPY_CLASS(wxNotificationIconEvtHandler);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxNotificationIconEvtHandler
// ----------------------------------------------------------------------------

wxNotificationIconEvtHandler::wxNotificationIconEvtHandler(wxTaskBarIcon *icon)
                            : m_icon(icon)
{
    m_icon->Connect
            (
              wxEVT_TASKBAR_BALLOON_TIMEOUT,
              wxTaskBarIconEventHandler(wxNotificationIconEvtHandler::OnTimeout),
              NULL,
              this
            );

    m_icon->Connect
            (
              wxEVT_TASKBAR_BALLOON_CLICK,
              wxTaskBarIconEventHandler(wxNotificationIconEvtHandler::OnClick),
              NULL,
              this
            );
}

void wxNotificationIconEvtHandler::OnIconHidden()
{
    delete m_icon;

    delete this;
}

void
wxNotificationIconEvtHandler::OnTimeout(wxTaskBarIconEvent& WXUNUSED(event))
{
    OnIconHidden();
}

void wxNotificationIconEvtHandler::OnClick(wxTaskBarIconEvent& WXUNUSED(event))
{
    // TODO: generate an event notifying the user code?

    OnIconHidden();
}

// ----------------------------------------------------------------------------
// wxBalloonNotifMsgImpl
// ----------------------------------------------------------------------------

wxTaskBarIcon *wxBalloonNotifMsgImpl::ms_iconToUse = NULL;

/* static */
wxTaskBarIcon *wxBalloonNotifMsgImpl::UseTaskBarIcon(wxTaskBarIcon *icon)
{
    wxTaskBarIcon * const iconOld = ms_iconToUse;
    ms_iconToUse = icon;
    return iconOld;
}

void wxBalloonNotifMsgImpl::SetUpIcon(wxWindow *win)
{
    if ( ms_iconToUse )
    {
        // use an existing icon
        m_ownsIcon = false;
        m_icon = ms_iconToUse;
    }
    else // no user-specified icon to attach to
    {
        // create our own one
        m_ownsIcon = true;
        m_icon = new wxTaskBarIcon;

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

        m_icon->SetIcon(icon);
    }
}

bool
wxBalloonNotifMsgImpl::DoShow(const wxString& title,
                              const wxString& message,
                              int timeout,
                              int flags)
{
    timeout *= 1000; // Windows expresses timeout in milliseconds

    return m_icon->ShowBalloon(title, message, timeout, flags);
}

// ----------------------------------------------------------------------------
// wxManualNotifMsgImpl
// ----------------------------------------------------------------------------

wxManualNotifMsgImpl::wxManualNotifMsgImpl(wxWindow *win)
                    : wxBalloonNotifMsgImpl(win),
                      m_win(win)
{
}

wxManualNotifMsgImpl::~wxManualNotifMsgImpl()
{
    if ( m_icon )
        DoClose();
}

bool
wxManualNotifMsgImpl::DoShow(const wxString& title,
                             const wxString& message,
                             int WXUNUSED_UNLESS_DEBUG(timeout),
                             int flags)
{
    wxASSERT_MSG( timeout == wxNotificationMessage::Timeout_Never,
                    wxT("shouldn't be used") );

    // base class creates the icon for us initially but we could have destroyed
    // it in DoClose(), recreate it if this was the case
    if ( !m_icon )
        SetUpIcon(m_win);

    // use maximal (in current Windows versions) timeout (but it will still
    // disappear on its own)
    return wxBalloonNotifMsgImpl::DoShow(title, message, 30, flags);
}

bool wxManualNotifMsgImpl::DoClose()
{
    if ( m_ownsIcon )
    {
        // we don't need the icon any more
        delete m_icon;
    }
    else // using an existing icon
    {
        // just hide the balloon
        m_icon->ShowBalloon("", "");
    }

    m_icon = NULL;

    return true;
}

// ----------------------------------------------------------------------------
// wxAutoNotifMsgImpl
// ----------------------------------------------------------------------------

wxAutoNotifMsgImpl::wxAutoNotifMsgImpl(wxWindow *win)
                  : wxBalloonNotifMsgImpl(win),
                    m_iconEvtHandler(new wxNotificationIconEvtHandler(m_icon))
{
}

bool
wxAutoNotifMsgImpl::DoShow(const wxString& title,
                           const wxString& message,
                           int timeout,
                           int flags)
{
    wxASSERT_MSG( timeout != wxNotificationMessage::Timeout_Never,
                    wxT("shouldn't be used") );

    if ( timeout == wxNotificationMessage::Timeout_Auto )
    {
        // choose a value more or less in the middle of the allowed range
        timeout = 1;
    }

    return wxBalloonNotifMsgImpl::DoShow(title, message, timeout, flags);
}

// ----------------------------------------------------------------------------
// wxNotificationMessage
// ----------------------------------------------------------------------------

/* static */
bool wxNotificationMessage::ms_alwaysUseGeneric = false;

/* static */
wxTaskBarIcon *wxNotificationMessage::UseTaskBarIcon(wxTaskBarIcon *icon)
{
    return wxBalloonNotifMsgImpl::UseTaskBarIcon(icon);
}

bool wxNotificationMessage::Show(int timeout)
{
    if ( !m_impl )
    {
        if ( !ms_alwaysUseGeneric && wxTheApp->GetShell32Version() >= 500 )
        {
            if ( timeout == Timeout_Never )
                m_impl = new wxManualNotifMsgImpl(GetParent());
            else
                m_impl = new wxAutoNotifMsgImpl(GetParent());
        }
        else // no support for balloon tooltips
        {
            m_impl = new wxGenericNotifMsgImpl;
        }
    }
    //else: reuse the same implementation for the subsequent calls, it would
    //      be too confusing if it changed

    return m_impl->DoShow(GetTitle(), GetMessage(), timeout, GetFlags());
}

bool wxNotificationMessage::Close()
{
    wxCHECK_MSG( m_impl, false, "must show the notification first" );

    return m_impl->DoClose();
}

wxNotificationMessage::~wxNotificationMessage()
{
    delete m_impl;
}

#endif // wxUSE_NOTIFICATION_MESSAGE && wxUSE_TASKBARICON
