///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/popupwin.cpp
// Purpose:     implements wxPopupWindow for wxMac
// Author:      Stefan Csomor
// Modified by:
// Created:
// Copyright:   (c) 2006 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// CAUTION : This is only experimental stuff right now

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_POPUPWIN

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#include "wx/popupwin.h"
#include "wx/tooltip.h"

#include "wx/osx/private.h"

// ============================================================================
// implementation
// ============================================================================

wxPopupWindow::~wxPopupWindow()
{
}

bool wxPopupWindow::Create(wxWindow *parent, int flags)
{
    // popup windows are created hidden by default
    Hide();

    return wxPopupWindowBase::Create(parent) &&
               wxNonOwnedWindow::Create(parent, wxID_ANY,
                                wxDefaultPosition, wxDefaultSize,
                                flags | wxPOPUP_WINDOW);

}

// under

bool wxPopupWindow::Show(bool show)
{
    if ( !wxWindow::Show(show) )
        return false;

    if ( m_nowpeer && show)
        m_nowpeer->ShowWithoutActivating();
    else if ( m_nowpeer )
        m_nowpeer->Show(false);

    if ( show )
    {
        // because apps expect a size event to occur at this moment
        wxSizeEvent event(GetSize() , m_windowId);
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }

    return true;
}


#endif // #if wxUSE_POPUPWIN
