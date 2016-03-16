/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/mdi.cpp
// Purpose:     MDI classes
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_MDI

#include "wx/mdi.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/menu.h"
    #include "wx/settings.h"
    #include "wx/statusbr.h"
#endif

#include "wx/osx/private.h"
#include "wx/osx/uma.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxMDIParentFrame, wxFrame);
wxIMPLEMENT_DYNAMIC_CLASS(wxMDIChildFrame, wxFrame);
wxIMPLEMENT_DYNAMIC_CLASS(wxMDIClientWindow, wxWindow);

wxBEGIN_EVENT_TABLE(wxMDIParentFrame, wxFrame)
    EVT_ACTIVATE(wxMDIParentFrame::OnActivate)
    EVT_SYS_COLOUR_CHANGED(wxMDIParentFrame::OnSysColourChanged)
wxEND_EVENT_TABLE()

#define TRACE_MDI "mdi"

static const int IDM_WINDOWTILEHOR  = 4001;
static const int IDM_WINDOWCASCADE = 4002;
static const int IDM_WINDOWICONS = 4003;
static const int IDM_WINDOWNEXT = 4004;
static const int IDM_WINDOWTILEVERT = 4005;

// others

void UMAHighlightAndActivateWindow( WindowRef inWindowRef , bool inActivate )
{
#if defined(wxOSX_USE_COCOA)
    wxUnusedVar(inActivate);
    wxUnusedVar(inWindowRef);
// TODO: implement me!
#endif
}

// ----------------------------------------------------------------------------
// Parent frame
// ----------------------------------------------------------------------------

void wxMDIParentFrame::Init()
{
    m_parentFrameActive = true;
    m_shouldBeShown = false;
}

bool wxMDIParentFrame::Create(wxWindow *parent,
    wxWindowID winid,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
{
    // this style can be used to prevent a window from having the standard MDI
    // "Window" menu
    if ( style & wxFRAME_NO_WINDOW_MENU )
    {
        m_windowMenu = NULL;
        style -= wxFRAME_NO_WINDOW_MENU ;
    }
    else // normal case: we have the window menu, so construct it
    {
        m_windowMenu = new wxMenu;

        m_windowMenu->Append(IDM_WINDOWCASCADE, wxT("&Cascade"));
        m_windowMenu->Append(IDM_WINDOWTILEHOR, wxT("Tile &Horizontally"));
        m_windowMenu->Append(IDM_WINDOWTILEVERT, wxT("Tile &Vertically"));
        m_windowMenu->AppendSeparator();
        m_windowMenu->Append(IDM_WINDOWICONS, wxT("&Arrange Icons"));
        m_windowMenu->Append(IDM_WINDOWNEXT, wxT("&Next"));
    }

    if ( !wxFrame::Create( parent , winid , title , pos , size , style , name ) )
        return false;

    m_parentFrameActive = true;

    m_clientWindow = OnCreateClient();
    if ( !m_clientWindow || !m_clientWindow->CreateClient(this, style) )
        return false;

    return true;
}

wxMDIParentFrame::~wxMDIParentFrame()
{
    DestroyChildren();

    // already deleted by DestroyChildren()
    m_clientWindow = NULL ;
}

void wxMDIParentFrame::SetMenuBar(wxMenuBar *menu_bar)
{
    wxFrame::SetMenuBar( menu_bar ) ;
}

void wxMDIParentFrame::GetRectForTopLevelChildren(int *x, int *y, int *w, int *h)
{
    if (x)
        *x = 0;
    if (y)
        *y = 0;

    wxDisplaySize(w, h);
}

void wxMDIParentFrame::AddChild(wxWindowBase *child)
{
    // moved this to front, so that we don't run into unset m_parent problems later
    wxFrame::AddChild(child);

    if ( !m_currentChild )
    {
        m_currentChild = wxDynamicCast(child, wxMDIChildFrame);

        if ( m_currentChild && IsShown() && !ShouldBeVisible() )
        {
            // we shouldn't remain visible any more
            wxFrame::Show(false);
            m_shouldBeShown = true;
        }
    }
}

void wxMDIParentFrame::RemoveChild(wxWindowBase *child)
{
    if ( child == m_currentChild )
    {
        // the current child isn't active any more, try to find another one
        m_currentChild = NULL;

        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxMDIChildFrame *
                childCur = wxDynamicCast(node->GetData(), wxMDIChildFrame);
            if ( childCur != child )
            {
                m_currentChild = childCur;
                break;
            }
        }
    }

    wxFrame::RemoveChild(child);

    // if there are no more children left we need to show the frame if we
    // hadn't shown it before because there were active children and it was
    // useless (note that we have to do it after fully removing the child, i.e.
    // after calling the base class RemoveChild() as otherwise we risk to touch
    // pointer to the child being deleted)
    if ( !m_currentChild && m_shouldBeShown && !IsShown() )
    {
        // we have to show it, but at least move it out of sight and make it of
        // smallest possible size (unfortunately (0, 0) doesn't work so that it
        // doesn't appear in expose
        SetSize(-10000, -10000, 1, 1);
        Show();
    }
}

void wxMDIParentFrame::MacActivate(long timestamp, bool activating)
{
    wxLogTrace(TRACE_MDI, wxT("MDI PARENT=%p MacActivate(0x%08lx,%s)"), this, timestamp, activating ? wxT("ACTIV") : wxT("deact"));

    if (activating)
    {
        if (s_macDeactivateWindow && s_macDeactivateWindow->GetParent() == this)
        {
            wxLogTrace(TRACE_MDI, wxT("child had been scheduled for deactivation, rehighlighting"));

            UMAHighlightAndActivateWindow((WindowRef)s_macDeactivateWindow->GetWXWindow(), true);

            wxLogTrace(TRACE_MDI, wxT("finished highliting child"));

            s_macDeactivateWindow = NULL;
        }
        else if (s_macDeactivateWindow == this)
        {
            wxLogTrace(TRACE_MDI, wxT("Avoided deactivation/activation of this=%p"), this);

            s_macDeactivateWindow = NULL;
        }
        else // window to deactivate is NULL or is not us or one of our kids
        {
            // activate kid instead
            if (m_currentChild)
                m_currentChild->MacActivate(timestamp, activating);
            else
                wxFrame::MacActivate(timestamp, activating);
        }
    }
    else
    {
        // We were scheduled for deactivation, and now we do it.
        if (s_macDeactivateWindow == this)
        {
            s_macDeactivateWindow = NULL;
            if (m_currentChild)
                m_currentChild->MacActivate(timestamp, activating);
            wxFrame::MacActivate(timestamp, activating);
        }
        else // schedule ourselves for deactivation
        {
            if (s_macDeactivateWindow)
            {
                wxLogTrace(TRACE_MDI, wxT("window=%p SHOULD have been deactivated, oh well!"), s_macDeactivateWindow);
            }
            wxLogTrace(TRACE_MDI, wxT("Scheduling delayed MDI Parent deactivation"));

            s_macDeactivateWindow = this;
        }
    }
}

void wxMDIParentFrame::OnActivate(wxActivateEvent& event)
{
    event.Skip();
}

// Responds to colour changes, and passes event on to children.
void wxMDIParentFrame::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    // TODO

    // Propagate the event to the non-top-level children
    wxFrame::OnSysColourChanged(event);
}

bool wxMDIParentFrame::ShouldBeVisible() const
{
    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext() )
    {
        wxWindow *win = node->GetData();

        if ( win->IsShown()
                && !wxDynamicCast(win, wxMDIChildFrame)
#if wxUSE_STATUSBAR
                && win != (wxWindow*) GetStatusBar()
#endif
                && win != GetClientWindow() )
        {
            // if we have a non-MDI child, do remain visible so that it could
            // be used
            return true;
        }
    }

    return false;
}

bool wxMDIParentFrame::Show( bool show )
{
    m_shouldBeShown = false;

    // don't really show the MDI frame unless it has any children other than
    // MDI children as it is pretty useless in this case

    if ( show )
    {
        if ( !ShouldBeVisible() && m_currentChild )
        {
            // don't make the window visible now but remember that we should
            // have had done it
            m_shouldBeShown = true;

            return false;
        }
    }

    return wxFrame::Show(show);
}

// ----------------------------------------------------------------------------
// Child frame
// ----------------------------------------------------------------------------

void wxMDIChildFrame::Init()
{
}

bool wxMDIChildFrame::Create(wxMDIParentFrame *parent,
                             wxWindowID winid,
                             const wxString& title,
                             const wxPoint& pos,
                             const wxSize& size,
                             long style,
                             const wxString& name)
{
    m_mdiParent = parent;

    SetName(name);

    if ( winid == wxID_ANY )
        winid = (int)NewControlId();

    wxNonOwnedWindow::Create( parent, winid, pos , size , MacRemoveBordersFromStyle(style) , name ) ;

    SetTitle( title );

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_APPWORKSPACE));

    return true;
}

wxMDIChildFrame::~wxMDIChildFrame()
{
    DestroyChildren();
}

void wxMDIChildFrame::MacActivate(long timestamp, bool activating)
{
    wxLogTrace(TRACE_MDI, wxT("MDI child=%p  MacActivate(0x%08lx,%s)"),this, timestamp, activating ? wxT("ACTIV") : wxT("deact"));

    wxMDIParentFrame *mdiparent = m_mdiParent;
    wxASSERT(mdiparent);

    if (activating)
    {
        if (s_macDeactivateWindow == m_parent)
        {
            wxLogTrace(TRACE_MDI, wxT("parent had been scheduled for deactivation, rehighlighting"));

            UMAHighlightAndActivateWindow((WindowRef)s_macDeactivateWindow->GetWXWindow(), true);

            wxLogTrace(TRACE_MDI, wxT("finished highliting parent"));

            s_macDeactivateWindow = NULL;
        }
        else if ((mdiparent->m_currentChild == this) || !s_macDeactivateWindow)
            mdiparent->wxFrame::MacActivate(timestamp, activating);

        if (mdiparent->m_currentChild && mdiparent->m_currentChild != this)
            mdiparent->m_currentChild->wxFrame::MacActivate(timestamp, false);
        mdiparent->m_currentChild = this;

        if (s_macDeactivateWindow == this)
        {
            wxLogTrace(TRACE_MDI, wxT("Avoided deactivation/activation of this=%p"), this);

            s_macDeactivateWindow = NULL;
        }
        else
            wxFrame::MacActivate(timestamp, activating);
    }
    else
    {
        // We were scheduled for deactivation, and now we do it.
        if (s_macDeactivateWindow == this)
        {
            s_macDeactivateWindow = NULL;
            wxFrame::MacActivate(timestamp, activating);
            if (mdiparent->m_currentChild == this)
                mdiparent->wxFrame::MacActivate(timestamp, activating);
        }
        else // schedule ourselves for deactivation
        {
            if (s_macDeactivateWindow)
            {
                wxLogTrace(TRACE_MDI, wxT("window=%p SHOULD have been deactivated, oh well!"), s_macDeactivateWindow);
            }
            wxLogTrace(TRACE_MDI, wxT("Scheduling delayed deactivation"));

            s_macDeactivateWindow = this;
        }
    }
}

// MDI operations
void wxMDIChildFrame::Activate()
{
    // The base class method calls Activate() so skip it to avoid infinite
    // recursion and go directly to the real Raise() implementation.
    wxFrame::Raise();
}

//-----------------------------------------------------------------------------
// wxMDIClientWindow
//-----------------------------------------------------------------------------

wxMDIClientWindow::~wxMDIClientWindow()
{
    DestroyChildren();
}

bool wxMDIClientWindow::CreateClient(wxMDIParentFrame *parent, long style)
{
    if ( !wxWindow::Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, style) )
        return false;

    return true;
}

void wxMDIClientWindow::DoGetClientSize(int *x, int *y) const
{
    wxDisplaySize( x , y ) ;
}

#endif // wxUSE_MDI
