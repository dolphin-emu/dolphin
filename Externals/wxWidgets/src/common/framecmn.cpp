/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/framecmn.cpp
// Purpose:     common (for all platforms) wxFrame functions
// Author:      Julian Smart, Vadim Zeitlin
// Created:     01/02/97
// Id:          $Id: framecmn.cpp 51463 2008-01-30 21:31:03Z VZ $
// Copyright:   (c) 1998 Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/frame.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/menuitem.h"
    #include "wx/dcclient.h"
    #include "wx/toolbar.h"
    #include "wx/statusbr.h"
#endif // WX_PRECOMP

// ----------------------------------------------------------------------------
// event table
// ----------------------------------------------------------------------------

#if wxUSE_MENUS && wxUSE_STATUSBAR

BEGIN_EVENT_TABLE(wxFrameBase, wxTopLevelWindow)
    EVT_MENU_OPEN(wxFrameBase::OnMenuOpen)
    EVT_MENU_CLOSE(wxFrameBase::OnMenuClose)

    EVT_MENU_HIGHLIGHT_ALL(wxFrameBase::OnMenuHighlight)
END_EVENT_TABLE()

#endif // wxUSE_MENUS && wxUSE_STATUSBAR

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// construction/destruction
// ----------------------------------------------------------------------------

wxFrameBase::wxFrameBase()
{
#if wxUSE_MENUS
    m_frameMenuBar = NULL;
#endif // wxUSE_MENUS

#if wxUSE_TOOLBAR
    m_frameToolBar = NULL;
#endif // wxUSE_TOOLBAR

#if wxUSE_STATUSBAR
    m_frameStatusBar = NULL;
#endif // wxUSE_STATUSBAR

    m_statusBarPane = 0;
}

wxFrameBase::~wxFrameBase()
{
    // this destructor is required for Darwin
}

wxFrame *wxFrameBase::New(wxWindow *parent,
                          wxWindowID id,
                          const wxString& title,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxString& name)
{
    return new wxFrame(parent, id, title, pos, size, style, name);
}

void wxFrameBase::DeleteAllBars()
{
#if wxUSE_MENUS
    if ( m_frameMenuBar )
    {
        delete m_frameMenuBar;
        m_frameMenuBar = (wxMenuBar *) NULL;
    }
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
    if ( m_frameStatusBar )
    {
        delete m_frameStatusBar;
        m_frameStatusBar = (wxStatusBar *) NULL;
    }
#endif // wxUSE_STATUSBAR

#if wxUSE_TOOLBAR
    if ( m_frameToolBar )
    {
        delete m_frameToolBar;
        m_frameToolBar = (wxToolBar *) NULL;
    }
#endif // wxUSE_TOOLBAR
}

bool wxFrameBase::IsOneOfBars(const wxWindow *win) const
{
#if wxUSE_MENUS
    if ( win == GetMenuBar() )
        return true;
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
    if ( win == GetStatusBar() )
        return true;
#endif // wxUSE_STATUSBAR

#if wxUSE_TOOLBAR
    if ( win == GetToolBar() )
        return true;
#endif // wxUSE_TOOLBAR

    return false;
}

// ----------------------------------------------------------------------------
// wxFrame size management: we exclude the areas taken by menu/status/toolbars
// from the client area, so the client area is what's really available for the
// frame contents
// ----------------------------------------------------------------------------

// get the origin of the client area in the client coordinates
wxPoint wxFrameBase::GetClientAreaOrigin() const
{
    wxPoint pt = wxTopLevelWindow::GetClientAreaOrigin();

#if wxUSE_TOOLBAR && !defined(__WXUNIVERSAL__)
    wxToolBar *toolbar = GetToolBar();
    if ( toolbar && toolbar->IsShown() )
    {
        int w, h;
        toolbar->GetSize(&w, &h);

        if ( toolbar->GetWindowStyleFlag() & wxTB_VERTICAL )
        {
            pt.x += w;
        }
        else
        {
            pt.y += h;
        }
    }
#endif // wxUSE_TOOLBAR

    return pt;
}


void wxFrameBase::SendSizeEvent()
{
    const wxSize size = GetSize();
    wxSizeEvent event( size, GetId() );
    event.SetEventObject( this );
    GetEventHandler()->AddPendingEvent( event );

#ifdef __WXGTK__
    // SendSizeEvent is typically called when a toolbar is shown
    // or hidden, but sending the size event alone is not enough
    // to trigger a full layout.
    ((wxFrame*)this)->GtkOnSize(
#ifndef __WXGTK20__
        0, 0, size.x, size.y
#endif // __WXGTK20__
    );
#endif // __WXGTK__
}


// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

bool wxFrameBase::ProcessCommand(int id)
{
#if wxUSE_MENUS
    wxMenuBar *bar = GetMenuBar();
    if ( !bar )
        return false;

    wxCommandEvent commandEvent(wxEVT_COMMAND_MENU_SELECTED, id);
    commandEvent.SetEventObject(this);

    wxMenuItem *item = bar->FindItem(id);
    if (item)
    {
        if (!item->IsEnabled())
            return true;

        if ((item->GetKind() == wxITEM_RADIO) && item->IsChecked() )
            return true;

        if (item->IsCheckable())
        {
            item->Toggle();

            // use the new value
            commandEvent.SetInt(item->IsChecked());
        }
    }

    return GetEventHandler()->ProcessEvent(commandEvent);
#else // !wxUSE_MENUS
    return false;
#endif // wxUSE_MENUS/!wxUSE_MENUS
}

// Do the UI update processing for this window. This is
// provided for the application to call if it wants to
// force a UI update, particularly for the menus and toolbar.
void wxFrameBase::UpdateWindowUI(long flags)
{
    wxWindowBase::UpdateWindowUI(flags);

#if wxUSE_TOOLBAR
    if (GetToolBar())
        GetToolBar()->UpdateWindowUI(flags);
#endif

#if wxUSE_MENUS
    if (GetMenuBar())
    {
        if ((flags & wxUPDATE_UI_FROMIDLE) && !wxUSE_IDLEMENUUPDATES)
        {
            // If coming from an idle event, we only
            // want to update the menus if we're
            // in the wxUSE_IDLEMENUUPDATES configuration:
            // so if we're not, do nothing
        }
        else
            DoMenuUpdates();
    }
#endif // wxUSE_MENUS
}

// ----------------------------------------------------------------------------
// event handlers for status bar updates from menus
// ----------------------------------------------------------------------------

#if wxUSE_MENUS && wxUSE_STATUSBAR

void wxFrameBase::OnMenuHighlight(wxMenuEvent& event)
{
#if wxUSE_STATUSBAR
    (void)ShowMenuHelp(GetStatusBar(), event.GetMenuId());
#endif // wxUSE_STATUSBAR
}

#if !wxUSE_IDLEMENUUPDATES
void wxFrameBase::OnMenuOpen(wxMenuEvent& event)
#else
void wxFrameBase::OnMenuOpen(wxMenuEvent& WXUNUSED(event))
#endif
{
#if !wxUSE_IDLEMENUUPDATES
    DoMenuUpdates(event.GetMenu());
#endif // !wxUSE_IDLEMENUUPDATES
}

void wxFrameBase::OnMenuClose(wxMenuEvent& WXUNUSED(event))
{
    // do we have real status text to restore?
    if ( !m_oldStatusText.empty() )
    {
        if ( m_statusBarPane >= 0 )
        {
            wxStatusBar *statbar = GetStatusBar();
            if ( statbar )
                statbar->SetStatusText(m_oldStatusText, m_statusBarPane);
        }

        m_oldStatusText.clear();
    }
}

#endif // wxUSE_MENUS && wxUSE_STATUSBAR

// Implement internal behaviour (menu updating on some platforms)
void wxFrameBase::OnInternalIdle()
{
    wxTopLevelWindow::OnInternalIdle();

#if wxUSE_MENUS && wxUSE_IDLEMENUUPDATES
    if (wxUpdateUIEvent::CanUpdate(this))
        DoMenuUpdates();
#endif
}

// ----------------------------------------------------------------------------
// status bar stuff
// ----------------------------------------------------------------------------

#if wxUSE_STATUSBAR

wxStatusBar* wxFrameBase::CreateStatusBar(int number,
                                          long style,
                                          wxWindowID id,
                                          const wxString& name)
{
    // the main status bar can only be created once (or else it should be
    // deleted before calling CreateStatusBar() again)
    wxCHECK_MSG( !m_frameStatusBar, (wxStatusBar *)NULL,
                 wxT("recreating status bar in wxFrame") );

    SetStatusBar(OnCreateStatusBar(number, style, id, name));

    return m_frameStatusBar;
}

wxStatusBar *wxFrameBase::OnCreateStatusBar(int number,
                                            long style,
                                            wxWindowID id,
                                            const wxString& name)
{
    wxStatusBar *statusBar = new wxStatusBar(this, id, style, name);

    statusBar->SetFieldsCount(number);

    return statusBar;
}

void wxFrameBase::SetStatusText(const wxString& text, int number)
{
    wxCHECK_RET( m_frameStatusBar != NULL, wxT("no statusbar to set text for") );

    m_frameStatusBar->SetStatusText(text, number);
}

void wxFrameBase::SetStatusWidths(int n, const int widths_field[] )
{
    wxCHECK_RET( m_frameStatusBar != NULL, wxT("no statusbar to set widths for") );

    m_frameStatusBar->SetStatusWidths(n, widths_field);

    PositionStatusBar();
}

void wxFrameBase::PushStatusText(const wxString& text, int number)
{
    wxCHECK_RET( m_frameStatusBar != NULL, wxT("no statusbar to set text for") );

    m_frameStatusBar->PushStatusText(text, number);
}

void wxFrameBase::PopStatusText(int number)
{
    wxCHECK_RET( m_frameStatusBar != NULL, wxT("no statusbar to set text for") );

    m_frameStatusBar->PopStatusText(number);
}

bool wxFrameBase::ShowMenuHelp(wxStatusBar *WXUNUSED(statbar), int menuId)
{
#if wxUSE_MENUS
    // if no help string found, we will clear the status bar text
    wxString helpString;
    bool show = menuId != wxID_SEPARATOR && menuId != -2 /* wxID_TITLE */;

    if ( show )
    {
        wxMenuBar *menuBar = GetMenuBar();
        if ( menuBar )
        {
            // it's ok if we don't find the item because it might belong
            // to the popup menu
            wxMenuItem *item = menuBar->FindItem(menuId);
            if ( item )
                helpString = item->GetHelp();
        }
    }

    DoGiveHelp(helpString, show);

    return !helpString.empty();
#else // !wxUSE_MENUS
    return false;
#endif // wxUSE_MENUS/!wxUSE_MENUS
}

void wxFrameBase::SetStatusBar(wxStatusBar *statBar)
{
    bool hadBar = m_frameStatusBar != NULL;
    m_frameStatusBar = statBar;

    if ( (m_frameStatusBar != NULL) != hadBar )
    {
        PositionStatusBar();

        DoLayout();
    }
}

#endif // wxUSE_STATUSBAR

#if wxUSE_MENUS || wxUSE_TOOLBAR
void wxFrameBase::DoGiveHelp(const wxString& text, bool show)
{
#if wxUSE_STATUSBAR
    if ( m_statusBarPane < 0 )
    {
        // status bar messages disabled
        return;
    }

    wxStatusBar *statbar = GetStatusBar();
    if ( !statbar )
        return;

    wxString help;
    if ( show )
    {
        help = text;

        // remember the old status bar text if this is the first time we're
        // called since the menu has been opened as we're going to overwrite it
        // in our DoGiveHelp() and we want to restore it when the menu is
        // closed
        //
        // note that it would be logical to do this in OnMenuOpen() but under
        // MSW we get an EVT_MENU_HIGHLIGHT before EVT_MENU_OPEN, strangely
        // enough, and so this doesn't work and instead we use the ugly trick
        // with using special m_oldStatusText value as "menu opened" (but it is
        // arguably better than adding yet another member variable to wxFrame
        // on all platforms)
        if ( m_oldStatusText.empty() )
        {
            m_oldStatusText = statbar->GetStatusText(m_statusBarPane);
            if ( m_oldStatusText.empty() )
            {
                // use special value to prevent us from doing this the next time
                m_oldStatusText += _T('\0');
            }
        }
    }
    else // hide the status bar text
    {
        // i.e. restore the old one
        help = m_oldStatusText;

        // make sure we get the up to date text when showing it the next time
        m_oldStatusText.clear();
    }

    statbar->SetStatusText(help, m_statusBarPane);
#else
    wxUnusedVar(text);
    wxUnusedVar(show);
#endif // wxUSE_STATUSBAR
}
#endif // wxUSE_MENUS || wxUSE_TOOLBAR


// ----------------------------------------------------------------------------
// toolbar stuff
// ----------------------------------------------------------------------------

#if wxUSE_TOOLBAR

wxToolBar* wxFrameBase::CreateToolBar(long style,
                                      wxWindowID id,
                                      const wxString& name)
{
    // the main toolbar can't be recreated (unless it was explicitly deleted
    // before)
    wxCHECK_MSG( !m_frameToolBar, (wxToolBar *)NULL,
                 wxT("recreating toolbar in wxFrame") );

    if ( style == -1 )
    {
        // use default style
        //
        // NB: we don't specify the default value in the method declaration
        //     because
        //      a) this allows us to have different defaults for different
        //         platforms (even if we don't have them right now)
        //      b) we don't need to include wx/toolbar.h in the header then
        style = wxBORDER_NONE | wxTB_HORIZONTAL | wxTB_FLAT;
    }

    SetToolBar(OnCreateToolBar(style, id, name));

    return m_frameToolBar;
}

wxToolBar* wxFrameBase::OnCreateToolBar(long style,
                                        wxWindowID id,
                                        const wxString& name)
{
#if defined(__WXWINCE__) && defined(__POCKETPC__)
    return new wxToolMenuBar(this, id,
                         wxDefaultPosition, wxDefaultSize,
                         style, name);
#else
    return new wxToolBar(this, id,
                         wxDefaultPosition, wxDefaultSize,
                         style, name);
#endif
}

void wxFrameBase::SetToolBar(wxToolBar *toolbar)
{
    bool hadBar = m_frameToolBar != NULL;
    m_frameToolBar = toolbar;

    if ( (m_frameToolBar != NULL) != hadBar )
    {
        PositionToolBar();

        DoLayout();
    }
}

#endif // wxUSE_TOOLBAR

// ----------------------------------------------------------------------------
// menus
// ----------------------------------------------------------------------------

#if wxUSE_MENUS

// update all menus
void wxFrameBase::DoMenuUpdates(wxMenu* menu)
{
    if (menu)
    {
        wxEvtHandler* source = GetEventHandler();
        menu->UpdateUI(source);
    }
    else
    {
        wxMenuBar* bar = GetMenuBar();
        if (bar != NULL)
            bar->UpdateMenus();
    }
}

void wxFrameBase::DetachMenuBar()
{
    if ( m_frameMenuBar )
    {
        m_frameMenuBar->Detach();
        m_frameMenuBar = NULL;
    }
}

void wxFrameBase::AttachMenuBar(wxMenuBar *menubar)
{
    if ( menubar )
    {
        menubar->Attach((wxFrame *)this);
        m_frameMenuBar = menubar;
    }
}

void wxFrameBase::SetMenuBar(wxMenuBar *menubar)
{
    if ( menubar == GetMenuBar() )
    {
        // nothing to do
        return;
    }

    DetachMenuBar();

    this->AttachMenuBar(menubar);
}

#endif // wxUSE_MENUS
