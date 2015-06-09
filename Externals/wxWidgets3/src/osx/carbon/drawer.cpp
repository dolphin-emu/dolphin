/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/drawer.cpp
// Purpose:     Drawer child window classes.
//              Drawer windows appear under their parent window and
//              behave like a drawer, opening and closing to reveal
//              content that does not need to be visible at all times.
// Author:      Jason Bagley
// Modified by: Ryan Norton (To make it work :), plus bug fixes)
// Created:     2004-30-01
// Copyright:   (c) Jason Bagley; Art & Logic, Inc.
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/osx/private.h"

#if defined( __WXMAC__ )

#include "wx/osx/carbon/drawer.h"

IMPLEMENT_DYNAMIC_CLASS(wxDrawerWindow, wxWindow)

// Use constants for now.
// These can be made into member variables and set dynamically.
const int kLeadingOffset = 20;
const int kTrailingOffset = 20;


// Converts Mac window edge constants to wxDirections, wxLEFT, wxRIGHT, etc.
static wxDirection WindowEdgeToDirection(OptionBits edge);

// Convert wxDirections to MAc window edge constants.
static OptionBits DirectionToWindowEdge(wxDirection direction);


wxDrawerWindow::wxDrawerWindow()
{
}

wxDrawerWindow::~wxDrawerWindow()
{
    SendDestroyEvent();
    Show(FALSE);
}

bool wxDrawerWindow::Create(wxWindow *parent,
 wxWindowID id, const wxString& WXUNUSED(title),
 wxSize size, wxDirection edge, const wxString& name)
{
    wxASSERT_MSG(NULL != parent, wxT("wxDrawerWindows must be attached to a parent window."));

    // Constrain the drawer size to the parent window.
    const wxSize parentSize(parent->GetClientSize());
    if (wxLEFT == edge || wxRIGHT == edge)
    {
        if (size.GetHeight() > parentSize.GetHeight())
            size.SetHeight(parentSize.GetHeight() - (kLeadingOffset + kTrailingOffset));
    }
    else
    {
        if (size.GetWidth() > parentSize.GetWidth())
            size.SetWidth(parentSize.GetWidth() - (kLeadingOffset + kTrailingOffset));
    }

    // Create the drawer window.
    const wxPoint pos(0, 0);
    const wxSize dummySize(0,0);
    const long style = wxFRAME_DRAWER;

    bool success  = wxNonOwnedWindow::Create(parent, id, pos, size, style, name);
    if (success)
    {
        // this->MacCreateRealWindow(pos, size, style, name);
        success = (GetWXWindow() != NULL);
    }

    if (success)
    {
        // Use drawer brush.
        SetBackgroundColour( wxColour( wxMacCreateCGColorFromHITheme( kThemeBrushDrawerBackground ) ) );
        ::SetThemeWindowBackground((WindowRef)GetWXWindow(), kThemeBrushDrawerBackground, false);

        // Leading and trailing offset are gaps from parent window edges
        // to where the drawer starts.
        ::SetDrawerOffsets((WindowRef)GetWXWindow() , kLeadingOffset, kTrailingOffset);

        // Set the drawers parent.
        // Is there a better way to get the parent's WindowRef?
        wxTopLevelWindow* tlwParent = wxDynamicCast(parent, wxTopLevelWindow);
        if (NULL != tlwParent)
        {
            OSStatus status = ::SetDrawerParent((WindowRef) GetWXWindow(),
            (WindowRef)tlwParent->GetWXWindow());
            success = (noErr == status);
        }
        else
            success = false;
    }

    return success && SetPreferredEdge(edge);
}

wxDirection wxDrawerWindow::GetCurrentEdge() const
{
    const OptionBits edge = ::GetDrawerCurrentEdge((WindowRef)GetWXWindow());
    return WindowEdgeToDirection(edge);
}

wxDirection wxDrawerWindow::GetPreferredEdge() const
{
    const OptionBits edge = ::GetDrawerPreferredEdge((WindowRef)GetWXWindow());
    return WindowEdgeToDirection(edge);
}

bool wxDrawerWindow::IsOpen() const
{
    WindowDrawerState state = ::GetDrawerState((WindowRef)GetWXWindow());
    return (state == kWindowDrawerOpen || state == kWindowDrawerOpening);
}

bool wxDrawerWindow::Open(bool show)
{
    static const Boolean kAsynchronous = true;
    OSStatus status = noErr;

    if (show)
    {
        const OptionBits preferredEdge = ::GetDrawerPreferredEdge((WindowRef)GetWXWindow());
        status = ::OpenDrawer((WindowRef)GetWXWindow(), preferredEdge, kAsynchronous);
    }
    else
        status = ::CloseDrawer((WindowRef)GetWXWindow(), kAsynchronous);

    return (noErr == status);
}

bool wxDrawerWindow::SetPreferredEdge(wxDirection edge)
{
    const OSStatus status = ::SetDrawerPreferredEdge((WindowRef)GetWXWindow(),
     DirectionToWindowEdge(edge));
    return (noErr == status);
}


OptionBits DirectionToWindowEdge(wxDirection direction)
{
    OptionBits edge;
    switch (direction)
    {
        case wxTOP:
        edge = kWindowEdgeTop;
        break;

        case wxBOTTOM:
        edge = kWindowEdgeBottom;
        break;

        case wxRIGHT:
        edge = kWindowEdgeRight;
        break;

        case wxLEFT:
        default:
        edge = kWindowEdgeLeft;
        break;
    }
    return edge;
}

wxDirection WindowEdgeToDirection(OptionBits edge)
{
    wxDirection direction;
    switch (edge)
    {
        case kWindowEdgeTop:
        direction = wxTOP;
        break;

        case kWindowEdgeBottom:
        direction = wxBOTTOM;
        break;

        case kWindowEdgeRight:
        direction = wxRIGHT;
        break;

        case kWindowEdgeDefault: // store current preferred and return that here?
        case kWindowEdgeLeft:
        default:
        direction = wxLEFT;
        break;
    }

    return direction;
}

#endif // defined( __WXMAC__ )
