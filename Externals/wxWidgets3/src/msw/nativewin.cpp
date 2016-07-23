///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/nativewin.cpp
// Purpose:     wxNativeWindow implementation
// Author:      Vadim Zeitlin
// Created:     2008-03-05
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwindows.org>
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

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include "wx/nativewin.h"
#include "wx/msw/private.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxNativeWindow
// ----------------------------------------------------------------------------

bool
wxNativeWindow::Create(wxWindow* parent,
                       wxWindowID winid,
                       wxNativeWindowHandle hwnd)
{
    wxCHECK_MSG( hwnd, false, wxS("Invalid null HWND") );
    wxCHECK_MSG( parent, false, wxS("Must have a valid parent") );
    wxASSERT_MSG( ::GetParent(hwnd) == GetHwndOf(parent),
                  wxS("The native window has incorrect parent") );

    const wxRect r = wxRectFromRECT(wxGetWindowRect(hwnd));

    // Skip wxWindow::Create() which would try to create a new HWND, we don't
    // want this as we already have one.
    if ( !CreateBase(parent, winid,
                     r.GetPosition(), r.GetSize(),
                     0, wxDefaultValidator, wxS("nativewindow")) )
        return false;

    parent->AddChild(this);

    SubclassWin(hwnd);

    if ( winid == wxID_ANY )
    {
        // We allocated a new ID to the control, use it at Windows level as
        // well because we assume that our and MSW IDs are the same in many
        // places and it seems prudent to avoid breaking this assumption.
        SetId(GetId());
    }
    else // We used a fixed ID.
    {
        // For the same reason as above, check that it's the same as the one
        // used by the native HWND.
        wxASSERT_MSG( ::GetWindowLong(hwnd, GWL_ID) == winid,
                      wxS("Mismatch between wx and native IDs") );
    }

    InheritAttributes();

    return true;
}

void wxNativeWindow::DoDisown()
{
    // We don't do anything here, clearing m_ownedByUser flag is enough.
}

wxNativeWindow::~wxNativeWindow()
{
    // Restore the original window proc and reset HWND to 0 to prevent it from
    // being destroyed in the base class dtor if it's owned by user code.
    if ( m_ownedByUser )
        UnsubclassWin();
}

// ----------------------------------------------------------------------------
// wxNativeContainerWindow
// ----------------------------------------------------------------------------

bool wxNativeContainerWindow::Create(wxNativeContainerWindowHandle hwnd)
{
    if ( !::IsWindow(hwnd) )
    {
        // strictly speaking, the fact that IsWindow() returns true doesn't
        // mean that the window handle is valid -- it could be being deleted
        // right now, for example
        //
        // but if it returns false, the handle is definitely invalid
        return false;
    }

    // make this HWND really a wxWindow
    SubclassWin(hwnd);

    // inherit the other attributes we can from the native HWND
    AdoptAttributesFromHWND();

    return true;
}

bool wxNativeContainerWindow::IsShown() const
{
    return (IsWindowVisible(static_cast<HWND>(m_hWnd)) != 0);
}

void wxNativeContainerWindow::OnNativeDestroyed()
{
    // don't use Close() or even Destroy() here, we really don't want to keep
    // an object using a no more existing HWND around for longer than necessary
    delete this;
}

WXLRESULT wxNativeContainerWindow::MSWWindowProc(WXUINT nMsg,
                                                 WXWPARAM wParam,
                                                 WXLPARAM lParam)
{
    switch ( nMsg )
    {
        case WM_CLOSE:
            // wxWindow itself, unlike wxFrame, doesn't react to WM_CLOSE and
            // just ignores it without even passing it to DefWindowProc(),
            // which means that the original WM_CLOSE handler wouldn't be
            // called if we didn't explicitly do it here.
            return MSWDefWindowProc(nMsg, wParam, lParam);

        case WM_DESTROY:
            // Send it to the original handler which may have some cleanup to
            // do as well. Notice that we must do it before calling
            // OnNativeDestroyed() as we can't use this object after doing it.
            MSWDefWindowProc(nMsg, wParam, lParam);

            OnNativeDestroyed();

            return 0;
    }

    return wxTopLevelWindow::MSWWindowProc(nMsg, wParam, lParam);
}

wxNativeContainerWindow::~wxNativeContainerWindow()
{
    // prevent the base class dtor from destroying the window, it doesn't
    // belong to us so we should leave it alive
    DissociateHandle();
}

