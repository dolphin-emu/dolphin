///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/notifmsgg.cpp
// Purpose:     generic implementation of wxGenericNotificationMessage
// Author:      Vadim Zeitlin
// Created:     2007-11-24
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

#if wxUSE_NOTIFICATION_MESSAGE

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/timer.h"
    #include "wx/sizer.h"
    #include "wx/statbmp.h"
    #include "wx/settings.h"
    #include "wx/panel.h"
#endif //WX_PRECOMP

#include "wx/artprov.h"
#include "wx/bmpbuttn.h"

// even if the platform has the native implementation, we still normally want
// to use the generic one (unless it's totally unsuitable for the target UI)
// because it may provide more features, so include
// wx/generic/notifmsg.h to get wxGenericNotificationMessage declaration even
// if wx/notifmsg.h only declares wxNotificationMessage itself (if it already
// uses the generic version, the second inclusion will do no harm)
#include "wx/notifmsg.h"
#include "wx/generic/notifmsg.h"
#include "wx/generic/private/notifmsg.h"
#include "wx/display.h"
#include "wx/textwrapper.h"

// ----------------------------------------------------------------------------
// wxNotificationMessageWindow
// ----------------------------------------------------------------------------

class wxNotificationMessageWindow : public wxFrame
{
public:
    wxNotificationMessageWindow(wxGenericNotificationMessageImpl* notificationImpl);

    virtual ~wxNotificationMessageWindow();

    void Set(int timeout);

    bool Hide();

    void SetMessageTitle(const wxString& title);

    void SetMessage(const wxString& message);

    void SetMessageIcon(const wxIcon& icon);

    bool AddAction(wxWindowID actionid, const wxString &label);

private:
    void OnClose(wxCloseEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnNotificationClicked(wxMouseEvent& event);
    void OnNotificationMouseEnter(wxMouseEvent& event);
    void OnNotificationMouseLeave(wxMouseEvent& event);
    void OnCloseClicked(wxCommandEvent& event);
    void OnActionButtonClicked(wxCommandEvent& event);

    // Dialog elements
    wxPanel* m_messagePanel;
    wxStaticBitmap* m_messageBmp;
    wxStaticText* m_messageText;
    wxStaticText* m_messageTitle;
    wxBitmapButton* m_closeBtn;
    wxBoxSizer* m_buttonSizer;

    wxTimer m_timer;
    int m_timeout;
    long m_timeoutTargetTime;
    int m_mouseActiveCount;

    wxGenericNotificationMessageImpl* m_notificationImpl;

    void PrepareNotificationControl(wxWindow* ctrl, bool handleClick = true);

    static wxPoint ms_presentationPos;

    static int ms_presentationDirection;

    static wxVector<wxNotificationMessageWindow*> ms_visibleNotifications;

    static void AddVisibleNotification(wxNotificationMessageWindow* notif);

    static void RemoveVisibleNotification(wxNotificationMessageWindow* notif);

    static void ResizeAndFitVisibleNotifications();

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxNotificationMessageWindow);
};

int wxNotificationMessageWindow::ms_presentationDirection = 0;
wxPoint wxNotificationMessageWindow::ms_presentationPos = wxDefaultPosition;

// ============================================================================
// wxNotificationMessageWindow implementation
// ============================================================================

wxBEGIN_EVENT_TABLE(wxNotificationMessageWindow, wxFrame)
    EVT_CLOSE(wxNotificationMessageWindow::OnClose)

    EVT_TIMER(wxID_ANY, wxNotificationMessageWindow::OnTimer)
wxEND_EVENT_TABLE()

wxVector<wxNotificationMessageWindow*> wxNotificationMessageWindow::ms_visibleNotifications;

wxNotificationMessageWindow::wxNotificationMessageWindow(wxGenericNotificationMessageImpl* notificationImpl)
                           : wxFrame(NULL, wxID_ANY, _("Notice"),
                                      wxDefaultPosition, wxDefaultSize,
                                      wxBORDER_NONE | wxFRAME_TOOL_WINDOW | wxSTAY_ON_TOP /* no caption, no border styles */),
                             m_timer(this),
                             m_mouseActiveCount(0),
                             m_notificationImpl(notificationImpl)
{
    m_buttonSizer = NULL;

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));

    m_messagePanel = new wxPanel(this, wxID_ANY);
    wxSizer * const msgSizer = new wxBoxSizer(wxHORIZONTAL);
    m_messagePanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    m_messagePanel->SetSizer(msgSizer);
    PrepareNotificationControl(m_messagePanel);

    // Add message icon to layout
    m_messageBmp = new wxStaticBitmap
        (
        m_messagePanel,
        wxID_ANY,
        wxArtProvider::GetMessageBoxIcon(wxICON_INFORMATION)
        );
    m_messageBmp->Hide();
    PrepareNotificationControl(m_messageBmp);
    msgSizer->Add(m_messageBmp, wxSizerFlags().Centre().DoubleBorder());

    // Create title and message sizers
    wxSizer* textSizer = new wxBoxSizer(wxVERTICAL);

    m_messageTitle = new wxStaticText(m_messagePanel, wxID_ANY, wxString());
    m_messageTitle->SetFont(m_messageTitle->GetFont().MakeBold());
    textSizer->Add(m_messageTitle, wxSizerFlags(0).Border());
    m_messageTitle->Hide();
    PrepareNotificationControl(m_messageTitle);

    m_messageText = new wxStaticText(m_messagePanel, wxID_ANY, wxString());
    textSizer->Add(m_messageText, wxSizerFlags(0).Border(wxLEFT | wxRIGHT | wxBOTTOM));
    PrepareNotificationControl(m_messageText);

    msgSizer->Add(textSizer, wxSizerFlags(1).Center());

    // Add a single close button if no actions are specified
    m_closeBtn = wxBitmapButton::NewCloseButton(m_messagePanel, wxID_ANY);
    msgSizer->Add(m_closeBtn, wxSizerFlags(0).Border(wxALL, 3).Top());
    m_closeBtn->Bind(wxEVT_BUTTON, &wxNotificationMessageWindow::OnCloseClicked, this);
    PrepareNotificationControl(m_closeBtn, false);

    wxSizer * const sizerTop = new wxBoxSizer(wxHORIZONTAL);
    sizerTop->Add(m_messagePanel, wxSizerFlags().Border(wxALL, FromDIP(1)));
    SetSizer(sizerTop);
}

wxNotificationMessageWindow::~wxNotificationMessageWindow()
{
    RemoveVisibleNotification(this);
}

void wxNotificationMessageWindow::PrepareNotificationControl(wxWindow* ctrl, bool handleClick)
{
    ctrl->Bind(wxEVT_ENTER_WINDOW, &wxNotificationMessageWindow::OnNotificationMouseEnter, this);
    ctrl->Bind(wxEVT_LEAVE_WINDOW, &wxNotificationMessageWindow::OnNotificationMouseLeave, this);

    if ( handleClick )
        ctrl->Bind(wxEVT_LEFT_DOWN, &wxNotificationMessageWindow::OnNotificationClicked, this);
}

void wxNotificationMessageWindow::SetMessageTitle(const wxString& title)
{
    m_messageTitle->SetLabelText(title);
    m_messageTitle->Show(!title.empty());
}

void wxNotificationMessageWindow::SetMessage(const wxString& message)
{
    m_messageText->SetLabelText(message);
    m_messageText->Show(!message.empty());
}

void wxNotificationMessageWindow::SetMessageIcon(const wxIcon& icon)
{
    m_messageBmp->SetBitmap(icon);
    m_messageBmp->Show(icon.IsOk());
}

bool wxNotificationMessageWindow::AddAction(wxWindowID actionid, const wxString &label)
{
    wxSizer* msgSizer = m_messagePanel->GetSizer();
    if ( m_buttonSizer == NULL )
    {
        msgSizer->Detach(m_closeBtn);
        m_closeBtn->Hide();
        m_buttonSizer = new wxBoxSizer(wxVERTICAL);
        msgSizer->Add(m_buttonSizer, wxSizerFlags(0).Center().Border());
    }

    wxButton* actionButton = new wxButton(m_messagePanel, actionid, label);
    actionButton->Bind(wxEVT_BUTTON, &wxNotificationMessageWindow::OnActionButtonClicked, this);
    PrepareNotificationControl(actionButton, false);
    int borderDir = (m_buttonSizer->GetChildren().empty()) ? 0 : wxTOP;
    m_buttonSizer->Add(actionButton, wxSizerFlags(0).Border(borderDir).Expand());

    return true;
}


bool wxNotificationMessageWindow::Hide()
{
    if ( m_timer.IsRunning() )
        m_timer.Stop();

    RemoveVisibleNotification(this);
    return wxFrame::HideWithEffect(wxSHOW_EFFECT_BLEND);
}

void wxNotificationMessageWindow::Set(int timeout)
{
    Layout();
    Fit();

    AddVisibleNotification(this);

    if ( timeout != wxGenericNotificationMessage::Timeout_Never )
    {
        // wxTimer uses ms, timeout is in seconds
        m_timer.Start(500);
        m_timeout = timeout;
        m_timeoutTargetTime = wxGetUTCTime() + timeout;
    }
    else if ( m_timer.IsRunning() )
    {
        m_timer.Stop();
    }
}

void wxNotificationMessageWindow::OnClose(wxCloseEvent& WXUNUSED(event))
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_DISMISSED);
    m_notificationImpl->ProcessNotificationEvent(evt);

    if ( m_timer.IsRunning() )
        m_timer.Stop();

    m_notificationImpl->Close();
}

void wxNotificationMessageWindow::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    if ( m_mouseActiveCount > 0 )
    {
        m_timeoutTargetTime = wxGetUTCTime() + m_timeout;
    }
    else if ( m_timeoutTargetTime != -1 &&
        wxGetUTCTime() >= m_timeoutTargetTime )
    {
        m_notificationImpl->Close();
    }
}

void wxNotificationMessageWindow::OnNotificationClicked(wxMouseEvent& WXUNUSED(event))
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_CLICK);
    m_notificationImpl->ProcessNotificationEvent(evt);

    m_notificationImpl->Close();
}

void wxNotificationMessageWindow::OnNotificationMouseEnter(wxMouseEvent& WXUNUSED(event))
{
    m_mouseActiveCount++;
}

void wxNotificationMessageWindow::OnNotificationMouseLeave(wxMouseEvent& WXUNUSED(event))
{
    m_mouseActiveCount--;
}

void wxNotificationMessageWindow::OnCloseClicked(wxCommandEvent& WXUNUSED(event))
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_DISMISSED);
    m_notificationImpl->ProcessNotificationEvent(evt);

    m_notificationImpl->Close();
}

void wxNotificationMessageWindow::OnActionButtonClicked(wxCommandEvent& event)
{
    wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_ACTION, event.GetId());
    m_notificationImpl->ProcessNotificationEvent(evt);

    m_notificationImpl->Close();
}

void wxNotificationMessageWindow::AddVisibleNotification(wxNotificationMessageWindow* notif)
{
    bool found = false;
    for ( wxVector<wxNotificationMessageWindow*>::iterator it = ms_visibleNotifications.begin();
        it != ms_visibleNotifications.end(); it++ )
    {
        if ( *it == notif )
        {
            found = true;
            break;
        }
    }

    if ( !found )
        ms_visibleNotifications.push_back(notif);

    ResizeAndFitVisibleNotifications();
}

void wxNotificationMessageWindow::RemoveVisibleNotification(wxNotificationMessageWindow* notif)
{
    for ( wxVector<wxNotificationMessageWindow*>::iterator it = ms_visibleNotifications.begin();
        it != ms_visibleNotifications.end(); it++ )
    {
        if ( *it == notif )
        {
            ms_visibleNotifications.erase(it);
            break;
        }
    }
    ResizeAndFitVisibleNotifications();
}

void wxNotificationMessageWindow::ResizeAndFitVisibleNotifications()
{
    if ( ms_presentationDirection == 0 )
    {
        // Determine presentation position

        wxDisplay display;
        wxRect clientArea = display.GetClientArea();
        wxRect geom = display.GetGeometry();
        if ( clientArea.y > 0 ) // Taskbar is at top
        {
            ms_presentationDirection = 1;
            ms_presentationPos = clientArea.GetTopRight();
        }
        else if ( clientArea.GetHeight() != geom.GetHeight() ) // Taskbar at bottom
        {
            ms_presentationDirection = -1;
            ms_presentationPos = clientArea.GetBottomRight();
        }
        else // Default to upper right screen corner with some padding
        {
            ms_presentationDirection = 1;
            ms_presentationPos.x = geom.GetWidth() - 30;
            ms_presentationPos.y = 30;
        }
    }

    int maxWidth = -1;

    // Determine max width
    for (wxVector<wxNotificationMessageWindow*>::iterator notif = ms_visibleNotifications.begin();
        notif != ms_visibleNotifications.end(); ++notif)
    {
        wxSize notifSize = (*notif)->GetSize();
        if ( notifSize.GetWidth() > maxWidth )
            maxWidth = notifSize.GetWidth();
    }

    int notifPadding = 2;

    wxPoint presentPos = ms_presentationPos;
    presentPos.x -= notifPadding + maxWidth;

    int prevNotifHeight = 0;

    for (wxVector<wxNotificationMessageWindow*>::iterator notif = ms_visibleNotifications.begin();
        notif != ms_visibleNotifications.end(); ++notif)
    {
        // Modify existing maxwidth
        wxSize notifSize = (*notif)->GetSize();
        if ( notifSize.GetWidth() < maxWidth )
        {
            notifSize.SetWidth(maxWidth);
            (*notif)->SetSize(notifSize);
            (*notif)->Layout();
        }

        if ( ms_presentationDirection > 0 )
        {
            presentPos.y += (notifPadding + prevNotifHeight);
            prevNotifHeight = notifSize.GetHeight();
        }
        else
        {
            presentPos.y -= (notifPadding + notifSize.GetHeight());
        }

        (*notif)->SetPosition(presentPos);
    }
}

// ============================================================================
// wxGenericNotificationMessage implementation
// ============================================================================

/* static */ void wxGenericNotificationMessage::SetDefaultTimeout(int timeout)
{
    wxGenericNotificationMessageImpl::SetDefaultTimeout(timeout);
}

void wxGenericNotificationMessage::Init()
{
    m_impl = new wxGenericNotificationMessageImpl(this);
}

// ----------------------------------------------------------------------------
// wxGenericNotificationMessageImpl
// ----------------------------------------------------------------------------

int wxGenericNotificationMessageImpl::ms_timeout = 3;

wxGenericNotificationMessageImpl::wxGenericNotificationMessageImpl(wxNotificationMessageBase* notification) :
    wxNotificationMessageImpl(notification)
{
    m_window = new wxNotificationMessageWindow(this);
}

wxGenericNotificationMessageImpl::~wxGenericNotificationMessageImpl()
{
    m_window->Destroy();
}

/* static */ void wxGenericNotificationMessageImpl::SetDefaultTimeout(int timeout)
{
    wxASSERT_MSG(timeout > 0,
        "negative or zero default timeout doesn't make sense");

    ms_timeout = timeout;
}

bool wxGenericNotificationMessageImpl::Show(int timeout)
{
    if ( timeout == wxNotificationMessageBase::Timeout_Auto )
    {
        timeout = GetDefaultTimeout();
    }

    SetActive(true);
    m_window->Set(timeout);

    m_window->ShowWithEffect(wxSHOW_EFFECT_BLEND);

    return true;
}

bool wxGenericNotificationMessageImpl::Close()
{
    if ( !m_window )
        return false;

    m_window->Hide();

    SetActive(false);

    return true;
}

void wxGenericNotificationMessageImpl::SetTitle(const wxString& title)
{
    m_window->SetMessageTitle(title);
}

void wxGenericNotificationMessageImpl::SetMessage(const wxString& message)
{
    m_window->SetMessage(message);
}

void wxGenericNotificationMessageImpl::SetParent(wxWindow *WXUNUSED(parent))
{

}

void wxGenericNotificationMessageImpl::SetFlags(int flags)
{
    m_window->SetMessageIcon( wxArtProvider::GetMessageBoxIcon(flags) );
}

void wxGenericNotificationMessageImpl::SetIcon(const wxIcon& icon)
{
    m_window->SetMessageIcon(icon);
}

bool wxGenericNotificationMessageImpl::AddAction(wxWindowID actionid, const wxString &label)
{
    return m_window->AddAction(actionid, label);
}


#endif // wxUSE_NOTIFICATION_MESSAGE
