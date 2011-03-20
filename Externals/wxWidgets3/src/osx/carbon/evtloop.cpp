///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/evtloop.cpp
// Purpose:     implementation of wxEventLoop for wxMac
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2006-01-12
// RCS-ID:      $Id: evtloop.cpp 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
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

#include "wx/evtloop.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#if wxUSE_GUI
#include "wx/nonownedwnd.h"
#endif

#include "wx/osx/private.h"

// ============================================================================
// wxEventLoop implementation
// ============================================================================

wxGUIEventLoop::wxGUIEventLoop()
{
}

static void DispatchAndReleaseEvent(EventRef theEvent)
{
    if ( wxTheApp )
        wxTheApp->MacSetCurrentEvent( theEvent, NULL );

    OSStatus status = SendEventToEventTarget(theEvent, GetEventDispatcherTarget());
    if (status == eventNotHandledErr && wxTheApp)
        wxTheApp->MacHandleUnhandledEvent(theEvent);

    ReleaseEvent( theEvent );
}

int wxGUIEventLoop::DoDispatchTimeout(unsigned long timeout)
{
    wxMacAutoreleasePool autoreleasepool;

    EventRef event;
    OSStatus status = ReceiveNextEvent(0, NULL, timeout/1000, true, &event);
    switch ( status )
    {
        default:
            wxFAIL_MSG( "unexpected ReceiveNextEvent() error" );
            // fall through

        case eventLoopTimedOutErr:
            return -1;

        case eventLoopQuitErr:
            // according to QA1061 this may also occur
            // when a WakeUp Process is executed
            return 0;

        case noErr:
            DispatchAndReleaseEvent(event);
            return 1;
    }
}

void wxGUIEventLoop::DoRun()
{
    wxMacAutoreleasePool autoreleasepool;
    RunApplicationEventLoop();
}

void wxGUIEventLoop::DoStop()
{
    QuitApplicationEventLoop();
}

CFRunLoopRef wxGUIEventLoop::CFGetCurrentRunLoop() const
{
    return wxCFEventLoop::CFGetCurrentRunLoop();
}

// TODO move into a evtloop_osx.cpp

wxModalEventLoop::wxModalEventLoop(wxWindow *modalWindow)
{
    m_modalWindow = wxDynamicCast(modalWindow, wxNonOwnedWindow);
    wxASSERT_MSG( m_modalWindow != NULL, "must pass in a toplevel window for modal event loop" );
    m_modalNativeWindow = m_modalWindow->GetWXWindow();
}

wxModalEventLoop::wxModalEventLoop(WXWindow modalNativeWindow)
{
    m_modalWindow = NULL;
    wxASSERT_MSG( modalNativeWindow != NULL, "must pass in a toplevel window for modal event loop" );
    m_modalNativeWindow = modalNativeWindow;
}

// END move into a evtloop_osx.cpp

void wxModalEventLoop::DoRun()
{
    wxWindowDisabler disabler(m_modalWindow);
    wxMacAutoreleasePool autoreleasepool;

    bool resetGroupParent = false;

    WindowGroupRef windowGroup = NULL;
    WindowGroupRef formerParentGroup = NULL;

    // make sure modal dialogs are in the right layer so that they are not covered
    if ( m_modalWindow != NULL )
    {
        if ( m_modalWindow->GetParent() == NULL )
        {
            windowGroup = GetWindowGroup(m_modalNativeWindow) ;
            if ( windowGroup != GetWindowGroupOfClass( kMovableModalWindowClass ) )
            {
                formerParentGroup = GetWindowGroupParent( windowGroup );
                SetWindowGroupParent( windowGroup, GetWindowGroupOfClass( kMovableModalWindowClass ) );
                resetGroupParent = true;
            }
        }
    }

    m_modalWindow->SetFocus();

    RunAppModalLoopForWindow(m_modalNativeWindow);

    if ( resetGroupParent )
    {
        SetWindowGroupParent( windowGroup , formerParentGroup );
    }

}

void wxModalEventLoop::DoStop()
{
    wxMacAutoreleasePool autoreleasepool;
    QuitAppModalLoopForWindow(m_modalNativeWindow);
}


