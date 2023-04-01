/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/frame.cpp
// Purpose:     wxFrame
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/frame.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/dcclient.h"
    #include "wx/menu.h"
    #include "wx/dialog.h"
    #include "wx/settings.h"
    #include "wx/toolbar.h"
    #include "wx/statusbr.h"
    #include "wx/menuitem.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

BEGIN_EVENT_TABLE(wxFrame, wxFrameBase)
  EVT_ACTIVATE(wxFrame::OnActivate)
  EVT_SYS_COLOUR_CHANGED(wxFrame::OnSysColourChanged)
END_EVENT_TABLE()

#define WX_MAC_STATUSBAR_HEIGHT 18

// ----------------------------------------------------------------------------
// creation/destruction
// ----------------------------------------------------------------------------

void wxFrame::Init()
{
    m_winLastFocused = NULL;
}

bool wxFrame::Create(wxWindow *parent,
           wxWindowID id,
           const wxString& title,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxString& name)
{
    if ( !wxTopLevelWindow::Create(parent, id, title, pos, size, style, name) )
        return false;

    return true;
}

wxFrame::~wxFrame()
{
    SendDestroyEvent();

    DeleteAllBars();
}

// get the origin of the client area in the client coordinates
wxPoint wxFrame::GetClientAreaOrigin() const
{
    wxPoint pt = wxTopLevelWindow::GetClientAreaOrigin();

#if wxUSE_TOOLBAR && !defined(__WXUNIVERSAL__)
    wxToolBar *toolbar = GetToolBar();
    if ( toolbar && toolbar->IsShown() )
    {
        int w, h;
        toolbar->GetSize(&w, &h);

        if ( toolbar->HasFlag(wxTB_LEFT) )
        {
            pt.x += w;
        }
        else if ( toolbar->HasFlag(wxTB_TOP) )
        {
#if !wxOSX_USE_NATIVE_TOOLBAR
            pt.y += h;
#endif
        }
    }
#endif

    return pt;
}

bool wxFrame::Enable(bool enable)
{
    if ( !wxWindow::Enable(enable) )
        return false;

#if wxUSE_MENUS
    // we should always enable/disable the menubar, even if we are not current, otherwise
    // we might miss some state change later (happened eg in the docview sample after PrintPreview)
    if ( m_frameMenuBar /*&& m_frameMenuBar == wxMenuBar::MacGetInstalledMenuBar()*/)
    {
        int iMaxMenu = m_frameMenuBar->GetMenuCount();
        for ( int i = 0 ; i < iMaxMenu ; ++ i )
        {
            m_frameMenuBar->EnableTop( i , enable ) ;
        }
    }
#endif
    return true;
}

#if wxUSE_STATUSBAR
wxStatusBar *wxFrame::OnCreateStatusBar(int number, long style, wxWindowID id,
    const wxString& name)
{
    wxStatusBar *statusBar;

    statusBar = new wxStatusBar(this, id, style, name);
    statusBar->SetSize(100, WX_MAC_STATUSBAR_HEIGHT);
    statusBar->SetFieldsCount(number);

    return statusBar;
}

void wxFrame::PositionStatusBar()
{
    if (m_frameStatusBar && m_frameStatusBar->IsShown() )
    {
        int w, h;
        GetClientSize(&w, &h);

        // Since we wish the status bar to be directly under the client area,
        // we use the adjusted sizes without using wxSIZE_NO_ADJUSTMENTS.
        m_frameStatusBar->SetSize(0, h, w, WX_MAC_STATUSBAR_HEIGHT);
    }
}
#endif // wxUSE_STATUSBAR

// Responds to colour changes, and passes event on to children.
void wxFrame::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_APPWORKSPACE));
    Refresh();

#if wxUSE_STATUSBAR
    if ( m_frameStatusBar )
    {
        wxSysColourChangedEvent event2;

        event2.SetEventObject( m_frameStatusBar );
        m_frameStatusBar->GetEventHandler()->ProcessEvent(event2);
    }
#endif // wxUSE_STATUSBAR

    // Propagate the event to the non-top-level children
    wxWindow::OnSysColourChanged(event);
}

// Default activation behaviour - set the focus for the first child
// subwindow found.
void wxFrame::OnActivate(wxActivateEvent& event)
{
    if ( !event.GetActive() )
    {
       // remember the last focused child if it is our child
        m_winLastFocused = FindFocus();

        // so we NULL it out if it's a child from some other frame
        wxWindow *win = m_winLastFocused;
        while ( win )
        {
            if ( win->IsTopLevel() )
            {
                if ( win != this )
                    m_winLastFocused = NULL;

                break;
            }

            win = win->GetParent();
        }

        event.Skip();
    }
    else
    {
        // restore focus to the child which was last focused
        wxWindow *parent = m_winLastFocused
            ? m_winLastFocused->GetParent()
            : NULL;

        if (parent == NULL)
            parent = this;

        wxSetFocusToChild(parent, &m_winLastFocused);

#if wxUSE_MENUS
        if (m_frameMenuBar != NULL)
        {
            m_frameMenuBar->MacInstallMenuBar();
        }
        else
        {
            wxFrame *tlf = wxDynamicCast( wxTheApp->GetTopWindow(), wxFrame );
            if (tlf != NULL)
            {
                // Trying top-level frame membar
                if (tlf->GetMenuBar())
                    tlf->GetMenuBar()->MacInstallMenuBar();
            }
        }
#endif
    }
}

#if wxUSE_MENUS
void wxFrame::DetachMenuBar()
{
    wxFrameBase::DetachMenuBar();
}

void wxFrame::AttachMenuBar( wxMenuBar *menuBar )
{
#if wxOSX_USE_CARBON
    wxFrame* tlf = wxDynamicCast( wxNonOwnedWindow::GetFromWXWindow( (WXWindow) FrontNonFloatingWindow() ) , wxFrame );
#elif wxOSX_USE_COCOA
    wxFrame* tlf = wxDynamicCast( wxNonOwnedWindow::GetFromWXWindow( wxOSXGetMainWindow() ) , wxFrame );
#else
    wxFrame* tlf = wxDynamicCast( wxTheApp->GetTopWindow(), wxFrame );
#endif
    bool makeCurrent = false;

    // if this is already the current menubar or we are the frontmost window
    if ( (tlf == this) || (m_frameMenuBar == wxMenuBar::MacGetInstalledMenuBar()) )
        makeCurrent = true;
    // or there is an app-level menubar like MDI
    else if ( tlf && (tlf->GetMenuBar() == NULL) && (((wxFrame*)wxTheApp->GetTopWindow()) == this) )
        makeCurrent = true;

    wxFrameBase::AttachMenuBar( menuBar );

    if (m_frameMenuBar)
    {
        if (makeCurrent)
            m_frameMenuBar->MacInstallMenuBar();
    }
}
#endif

void wxFrame::DoGetClientSize(int *x, int *y) const
{
    wxTopLevelWindow::DoGetClientSize( x , y );

#if wxUSE_STATUSBAR
    if ( GetStatusBar() && GetStatusBar()->IsShown() && y )
        *y -= WX_MAC_STATUSBAR_HEIGHT;
#endif

#if wxUSE_TOOLBAR
    wxToolBar *toolbar = GetToolBar();
    if ( toolbar && toolbar->IsShown() )
    {
        int w, h;
        toolbar->GetSize(&w, &h);

        if ( toolbar->IsVertical() )
        {
            if ( x )
                *x -= w;
        }
        else if ( toolbar->HasFlag( wxTB_BOTTOM ) )
        {
            if ( y )
                *y -= h;
        }
        else
        {
#if !wxOSX_USE_NATIVE_TOOLBAR
            if ( y )
                *y -= h;
#endif
        }
    }
#endif
}

bool wxFrame::MacIsChildOfClientArea( const wxWindow* child ) const
{
#if wxUSE_STATUSBAR
    if ( child == GetStatusBar() )
        return false ;
#endif

#if wxUSE_TOOLBAR
    if ( child == GetToolBar() )
        return false ;
#endif

    return wxFrameBase::MacIsChildOfClientArea( child ) ;
}

void wxFrame::DoSetClientSize(int clientwidth, int clientheight)
{
    int currentclientwidth , currentclientheight ;
    int currentwidth , currentheight ;

    GetClientSize( &currentclientwidth , &currentclientheight ) ;
    if ( clientwidth == -1 )
        clientwidth = currentclientwidth ;
    if ( clientheight == -1 )
        clientheight = currentclientheight ;
    GetSize( &currentwidth , &currentheight ) ;

    // find the current client size

    // Find the difference between the entire window (title bar and all) and
    // the client area; add this to the new client size to move the window
    DoSetSize( -1 , -1 , currentwidth + clientwidth - currentclientwidth ,
        currentheight + clientheight - currentclientheight , wxSIZE_USE_EXISTING ) ;
}

#if wxUSE_TOOLBAR
void wxFrame::SetToolBar(wxToolBar *toolbar)
{
    if ( m_frameToolBar == toolbar )
        return ;

#ifndef __WXOSX_IPHONE__
#if wxOSX_USE_NATIVE_TOOLBAR
    if ( m_frameToolBar )
        m_frameToolBar->MacInstallNativeToolbar( false ) ;
#endif
#endif
    m_frameToolBar = toolbar ;

#ifndef __WXOSX_IPHONE__
#if wxOSX_USE_NATIVE_TOOLBAR
    if ( toolbar )
        toolbar->MacInstallNativeToolbar( true ) ;
#endif
#endif
}

wxToolBar* wxFrame::CreateToolBar(long style, wxWindowID id, const wxString& name)
{
    if ( wxFrameBase::CreateToolBar(style, id, name) )
        PositionToolBar();

    return m_frameToolBar;
}

void wxFrame::PositionToolBar()
{
    int cw, ch;

    wxTopLevelWindow::DoGetClientSize( &cw , &ch );

    int statusX = 0 ;
    int statusY = 0 ;

#if wxUSE_STATUSBAR
    if (GetStatusBar() && GetStatusBar()->IsShown())
    {
        GetStatusBar()->GetSize(&statusX, &statusY);
        ch -= statusY;
    }
#endif

#ifdef __WXOSX_IPHONE__
    // TODO integrate this in a better way, on iphone the status bar is not a child of the content view
    // but the toolbar is
    ch -= 20;
#endif

    if (GetToolBar())
    {
        int tx, ty, tw, th;

        tx = ty = 0 ;
        GetToolBar()->GetSize(&tw, &th);
        if (GetToolBar()->HasFlag(wxTB_LEFT))
        {
            // Use the 'real' position. wxSIZE_NO_ADJUSTMENTS
            // means, pretend we don't have toolbar/status bar, so we
            // have the original client size.
            GetToolBar()->SetSize(tx , ty , tw, ch , wxSIZE_NO_ADJUSTMENTS );
        }
        else if (GetToolBar()->HasFlag(wxTB_RIGHT))
        {
            // Use the 'real' position. wxSIZE_NO_ADJUSTMENTS
            // means, pretend we don't have toolbar/status bar, so we
            // have the original client size.
            tx = cw - tw;
            GetToolBar()->SetSize(tx , ty , tw, ch , wxSIZE_NO_ADJUSTMENTS );
        }
        else if (GetToolBar()->HasFlag(wxTB_BOTTOM))
        {
            tx = 0;
            ty = ch - th;
            GetToolBar()->SetSize(tx, ty, cw, th, wxSIZE_NO_ADJUSTMENTS );
        }
        else
        {
#if !wxOSX_USE_NATIVE_TOOLBAR
            // Use the 'real' position
            GetToolBar()->SetSize(tx , ty , cw , th, wxSIZE_NO_ADJUSTMENTS );
#endif
        }
    }
}
#endif // wxUSE_TOOLBAR

void wxFrame::PositionBars()
{
#if wxUSE_STATUSBAR
    PositionStatusBar();
#endif
#if wxUSE_TOOLBAR
    PositionToolBar();
#endif
}


