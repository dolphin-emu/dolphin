///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/toplevel_osx.cpp
// Purpose:     implements wxTopLevelWindow for Mac
// Author:      Stefan Csomor
// Modified by:
// Created:     24.09.01
// Copyright:   (c) 2001-2004 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#include "wx/toplevel.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/frame.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/settings.h"
    #include "wx/strconv.h"
    #include "wx/control.h"
#endif //WX_PRECOMP

#include "wx/tooltip.h"
#include "wx/dnd.h"

#if wxUSE_SYSTEM_OPTIONS
    #include "wx/sysopt.h"
#endif

// for targeting OSX
#include "wx/osx/private.h"

// ============================================================================
// wxTopLevelWindowMac implementation
// ============================================================================

BEGIN_EVENT_TABLE(wxTopLevelWindowMac, wxTopLevelWindowBase)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxTopLevelWindowMac creation
// ----------------------------------------------------------------------------


void wxTopLevelWindowMac::Init()
{
    m_iconized =
    m_maximizeOnShow = false;
}

bool wxTopLevelWindowMac::Create(wxWindow *parent,
                                 wxWindowID id,
                                 const wxString& title,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style,
                                 const wxString& name)
{
    if ( !wxNonOwnedWindow::Create(parent, id, pos, size, style, name) )
        return false;

    wxWindow::SetLabel( title ) ;
    m_nowpeer->SetTitle(title, GetFont().GetEncoding() );
    wxTopLevelWindows.Append(this);

    return true;
}

bool wxTopLevelWindowMac::Create(wxWindow *parent,
                                 WXWindow nativeWindow)
{
    if ( !wxNonOwnedWindow::Create(parent, nativeWindow ) )
        return false;

    wxTopLevelWindows.Append(this);

    return true;
}

wxTopLevelWindowMac::~wxTopLevelWindowMac()
{
}

bool wxTopLevelWindowMac::Destroy()
{
    // NB: this will get called during destruction if we don't do it now,
    // and may fire a kill focus event on a control being destroyed
#if wxOSX_USE_CARBON
    if (m_nowpeer && m_nowpeer->GetWXWindow())
        ClearKeyboardFocus( (WindowRef)m_nowpeer->GetWXWindow() );
#endif
    // delayed destruction: the tlw will be deleted during the next idle
    // loop iteration
    if ( !wxPendingDelete.Member(this) )
        wxPendingDelete.Append(this);
    
    Hide();
    return true;
}


// ----------------------------------------------------------------------------
// wxTopLevelWindowMac maximize/minimize
// ----------------------------------------------------------------------------

void wxTopLevelWindowMac::Maximize(bool maximize)
{
    if ( IsMaximized() != maximize )
        m_nowpeer->Maximize(maximize);
}

bool wxTopLevelWindowMac::IsMaximized() const
{
    if ( m_nowpeer == NULL )
        return false;

    return m_nowpeer->IsMaximized();
}

void wxTopLevelWindowMac::Iconize(bool iconize)
{
    if ( IsIconized() != iconize )
        m_nowpeer->Iconize(iconize);
}

bool wxTopLevelWindowMac::IsIconized() const
{
    if ( m_nowpeer == NULL )
        return false;

    return m_nowpeer->IsIconized();
}

void wxTopLevelWindowMac::Restore()
{
    if ( IsMaximized() )
        Maximize(false);
    else if ( IsIconized() )
        Iconize(false);
}

// ----------------------------------------------------------------------------
// wxTopLevelWindowMac misc
// ----------------------------------------------------------------------------

wxPoint wxTopLevelWindowMac::GetClientAreaOrigin() const
{
    return wxPoint(0, 0) ;
}

void wxTopLevelWindowMac::SetTitle(const wxString& title)
{
    m_label = title ;

    if ( m_nowpeer )
        m_nowpeer->SetTitle(title, GetFont().GetEncoding() );
}

wxString wxTopLevelWindowMac::GetTitle() const
{
    return wxWindow::GetLabel();
}

void wxTopLevelWindowMac::ShowWithoutActivating()
{
    // wxTopLevelWindowBase is derived from wxNonOwnedWindow, so don't
    // call it here.
    if ( !wxWindow::Show(true) )
        return;

    m_nowpeer->ShowWithoutActivating();

    // because apps expect a size event to occur at this moment
    SendSizeEvent();
}

bool wxTopLevelWindowMac::ShowFullScreen(bool show, long style)
{
    return m_nowpeer->ShowFullScreen(show, style);
}

bool wxTopLevelWindowMac::IsFullScreen() const
{
    return m_nowpeer->IsFullScreen();
}

void wxTopLevelWindowMac::RequestUserAttention(int flags)
{
    return m_nowpeer->RequestUserAttention(flags);
}

bool wxTopLevelWindowMac::IsActive()
{
    return m_nowpeer->IsActive();
}

void wxTopLevelWindowMac::OSXSetModified(bool modified)
{
    m_nowpeer->SetModified(modified);
}

bool wxTopLevelWindowMac::OSXIsModified() const
{
    return m_nowpeer->IsModified();
}

void wxTopLevelWindowMac::SetRepresentedFilename(const wxString& filename)
{
    m_nowpeer->SetRepresentedFilename(filename);
}
