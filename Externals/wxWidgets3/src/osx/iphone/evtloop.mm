///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/evtloop.mm
// Purpose:     implementation of wxEventLoop for OS X
// Author:      Vadim Zeitlin, Stefan Csomor
// Modified by:
// Created:     2006-01-12
// RCS-ID:      $Id: evtloop.mm 67232 2011-03-18 15:10:15Z DS $
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
#endif // WX_PRECOMP

#include "wx/log.h"

#if wxUSE_GUI
    #include "wx/nonownedwnd.h"
#endif

#include "wx/osx/private.h"

// ============================================================================
// wxEventLoop implementation
// ============================================================================

/*
static int CalculateUIEventMaskFromEventCategory(wxEventCategory cat)
{
	NSLeftMouseDownMask	|
	NSLeftMouseUpMask |
	NSRightMouseDownMask |
	NSRightMouseUpMask		= 1 << NSRightMouseUp,
	NSMouseMovedMask		= 1 << NSMouseMoved,
	NSLeftMouseDraggedMask		= 1 << NSLeftMouseDragged,
	NSRightMouseDraggedMask		= 1 << NSRightMouseDragged,
	NSMouseEnteredMask		= 1 << NSMouseEntered,
	NSMouseExitedMask		= 1 << NSMouseExited,
        NSScrollWheelMask		= 1 << NSScrollWheel,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
	NSTabletPointMask		= 1 << NSTabletPoint,
	NSTabletProximityMask		= 1 << NSTabletProximity,
#endif
	NSOtherMouseDownMask		= 1 << NSOtherMouseDown,
	NSOtherMouseUpMask		= 1 << NSOtherMouseUp,
	NSOtherMouseDraggedMask		= 1 << NSOtherMouseDragged,



	NSKeyDownMask			= 1 << NSKeyDown,
	NSKeyUpMask			= 1 << NSKeyUp,
	NSFlagsChangedMask		= 1 << NSFlagsChanged,

	NSAppKitDefinedMask		= 1 << NSAppKitDefined,
	NSSystemDefinedMask		= 1 << NSSystemDefined,
	UIApplicationDefinedMask	= 1 << UIApplicationDefined,
	NSPeriodicMask			= 1 << NSPeriodic,
	NSCursorUpdateMask		= 1 << NSCursorUpdate,

	NSAnyEventMask			= 0xffffffffU
}
*/

wxGUIEventLoop::wxGUIEventLoop()
{
}

void wxGUIEventLoop::DoRun()
{
    if ( IsMain() )
    {
        wxMacAutoreleasePool pool;
        const char* appname = "app";
        UIApplicationMain( 1, (char**) &appname, nil, @"wxAppDelegate" );
    }
    else 
    {
        wxCFEventLoop::DoRun();
    }
}

int wxGUIEventLoop::DoDispatchTimeout(unsigned long timeout)
{
    return wxCFEventLoop::DoDispatchTimeout(timeout);
}

void wxGUIEventLoop::DoStop()
{
    return wxCFEventLoop::DoStop();
}

CFRunLoopRef wxGUIEventLoop::CFGetCurrentRunLoop() const
{
    return wxCFEventLoop::CFGetCurrentRunLoop();
}


// TODO move into a evtloop_osx.cpp

wxModalEventLoop::wxModalEventLoop(wxWindow *modalWindow)
{
    m_modalWindow = dynamic_cast<wxNonOwnedWindow*> (modalWindow);
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
    // presentModalViewController:animated:
}

void wxModalEventLoop::DoStop()
{
    // (void)dismissModalViewControllerAnimated:(BOOL)animated
}
