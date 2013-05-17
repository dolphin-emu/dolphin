///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/evtloop.mm
// Purpose:     implementation of wxEventLoop for OS X
// Author:      Vadim Zeitlin, Stefan Csomor
// Modified by:
// Created:     2006-01-12
// RCS-ID:      $Id: evtloop.mm 70786 2012-03-03 13:09:54Z SC $
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
    #include "wx/nonownedwnd.h"
#endif // WX_PRECOMP

#include "wx/log.h"

#include "wx/osx/private.h"

// ============================================================================
// wxEventLoop implementation
// ============================================================================

#if 0

// in case we want to integrate this

static NSUInteger CalculateNSEventMaskFromEventCategory(wxEventCategory cat)
{
    // the masking system doesn't really help, as only the lowlevel UI events
    // are split in a useful way, all others are way to broad
        
    if ( (cat | wxEVT_CATEGORY_USER_INPUT) && (cat | (~wxEVT_CATEGORY_USER_INPUT) ) )
        return NSAnyEventMask;
    
    NSUInteger mask = 0;

    if ( cat | wxEVT_CATEGORY_USER_INPUT )
    {
        mask |=
            NSLeftMouseDownMask	|
            NSLeftMouseUpMask |
            NSRightMouseDownMask |
            NSRightMouseUpMask |
            NSMouseMovedMask |
            NSLeftMouseDraggedMask |
            NSRightMouseDraggedMask |
            NSMouseEnteredMask |
            NSMouseExitedMask |
            NSScrollWheelMask |
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
            NSTabletPointMask |
            NSTabletProximityMask |
#endif
            NSOtherMouseDownMask |
            NSOtherMouseUpMask |
            NSOtherMouseDraggedMask |

            NSKeyDownMask |
            NSKeyUpMask |
            NSFlagsChangedMask |
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
            NSEventMaskGesture |
            NSEventMaskMagnify |
            NSEventMaskSwipe |
            NSEventMaskRotate |
            NSEventMaskBeginGesture |
            NSEventMaskEndGesture |
#endif
            0;
    }
    
    if ( cat | (~wxEVT_CATEGORY_USER_INPUT) )
    {
        mask |= 
            NSAppKitDefinedMask |
            NSSystemDefinedMask |
            NSApplicationDefinedMask |
            NSPeriodicMask |
            NSCursorUpdateMask;
    }
    
    return mask;
}

#endif

wxGUIEventLoop::wxGUIEventLoop()
{
    m_modalSession = nil;
    m_dummyWindow = nil;
    m_modalNestedLevel = 0;
    m_modalWindow = NULL;
}

wxGUIEventLoop::~wxGUIEventLoop()
{
    wxASSERT( m_modalSession == nil );
    wxASSERT( m_dummyWindow == nil );
    wxASSERT( m_modalNestedLevel == 0 );
}

//-----------------------------------------------------------------------------
// events dispatch and loop handling
//-----------------------------------------------------------------------------

#if 0

bool wxGUIEventLoop::Pending() const
{
#if 0
    // this code doesn't reliably detect pending events
    // so better return true and have the dispatch deal with it
    // as otherwise we end up in a tight loop when idle events are responded
    // to by RequestMore(true)
    wxMacAutoreleasePool autoreleasepool;
  
    return [[NSApplication sharedApplication]
            nextEventMatchingMask: NSAnyEventMask
            untilDate: nil
            inMode: NSDefaultRunLoopMode
            dequeue: NO] != nil;
#else
    return true;
#endif
}


bool wxGUIEventLoop::Dispatch()
{
    if ( !wxTheApp )
        return false;

    wxMacAutoreleasePool autoreleasepool;

    if(NSEvent *event = [NSApp
                nextEventMatchingMask:NSAnyEventMask
                untilDate:[NSDate dateWithTimeIntervalSinceNow: m_sleepTime]
                inMode:NSDefaultRunLoopMode
                dequeue: YES])
    {
        WXEVENTREF formerEvent = wxTheApp == NULL ? NULL : wxTheApp->MacGetCurrentEvent();
        WXEVENTHANDLERCALLREF formerHandler = wxTheApp == NULL ? NULL : wxTheApp->MacGetCurrentEventHandlerCallRef();

        if (wxTheApp)
            wxTheApp->MacSetCurrentEvent(event, NULL);
        m_sleepTime = 0.0;
        [NSApp sendEvent: event];

        if (wxTheApp)
            wxTheApp->MacSetCurrentEvent(formerEvent , formerHandler);
    }
    else
    {
        if (wxTheApp)
            wxTheApp->ProcessPendingEvents();
        
        if ( wxTheApp->ProcessIdle() )
            m_sleepTime = 0.0 ;
        else
        {
            m_sleepTime = 1.0;
#if wxUSE_THREADS
            wxMutexGuiLeave();
            wxMilliSleep(20);
            wxMutexGuiEnter();
#endif
        }
    }

    return true;
}

#endif

int wxGUIEventLoop::DoDispatchTimeout(unsigned long timeout)
{
    wxMacAutoreleasePool autoreleasepool;

    if ( m_modalSession )
    {
        NSInteger response = [NSApp runModalSession:(NSModalSession)m_modalSession];
        
        switch (response) 
        {
            case NSRunContinuesResponse:
            {
                if ( [[NSApplication sharedApplication]
                        nextEventMatchingMask: NSAnyEventMask
                        untilDate: nil
                        inMode: NSDefaultRunLoopMode
                        dequeue: NO] != nil )
                    return 1;
                
                return -1;
            }
                
            case NSRunStoppedResponse:
            case NSRunAbortedResponse:
                return -1;
            default:
                wxFAIL_MSG("unknown response code");
                break;
        }
        return -1;
    }
    else 
    {        
        NSEvent *event = [NSApp
                    nextEventMatchingMask:NSAnyEventMask
                    untilDate:[NSDate dateWithTimeIntervalSinceNow: timeout/1000]
                    inMode:NSDefaultRunLoopMode
                    dequeue: YES];
        
        if ( event == nil )
            return -1;

        [NSApp sendEvent: event];

        return 1;
    }
}

void wxGUIEventLoop::DoRun()
{
    wxMacAutoreleasePool autoreleasepool;
    [NSApp run];
}

void wxGUIEventLoop::DoStop()
{
    // only calling stop: is not enough when called from a runloop-observer,
    // therefore add a dummy event, to make sure the runloop gets another round
    [NSApp stop:0];
    WakeUp();
}

void wxGUIEventLoop::WakeUp()
{
    // NSEvent* cevent = [NSApp currentEvent];
    NSString* mode = [[NSRunLoop mainRunLoop] currentMode];
    
    // when already in a mouse event handler, don't add higher level event
    // if ( cevent != nil && [cevent type] <= NSMouseMoved && )
    if ( [NSEventTrackingRunLoopMode isEqualToString:mode] )
    {
        // NSLog(@"event for wakeup %@ in mode %@",cevent,mode);
        wxCFEventLoop::WakeUp();        
    }
    else
    {
        wxMacAutoreleasePool autoreleasepool;
        NSEvent *event = [NSEvent otherEventWithType:NSApplicationDefined 
                                        location:NSMakePoint(0.0, 0.0) 
                                   modifierFlags:0 
                                       timestamp:0 
                                    windowNumber:0 
                                         context:nil
                                         subtype:0 data1:0 data2:0]; 
        [NSApp postEvent:event atStart:FALSE];
    }
}

CFRunLoopRef wxGUIEventLoop::CFGetCurrentRunLoop() const
{
    NSRunLoop* nsloop = [NSRunLoop currentRunLoop];
    return [nsloop getCFRunLoop];
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
    wxMacAutoreleasePool pool;

    // If the app hasn't started, flush the event queue
    // If we don't do this, the Dock doesn't get the message that
    // the app has started so will refuse to activate it.
    [NSApplication sharedApplication];
    if (![NSApp isRunning])
    {
        while(NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES])
        {
            [NSApp sendEvent:event];
        }
    }
    
    [NSApp runModalForWindow:m_modalNativeWindow];
}

void wxModalEventLoop::DoStop()
{
    [NSApp abortModal];
}

void wxGUIEventLoop::BeginModalSession( wxWindow* modalWindow )
{
    WXWindow nsnow = nil;

    if ( m_modalNestedLevel > 0 )
    {
        wxASSERT_MSG( m_modalWindow == modalWindow, "Nested Modal Sessions must be based on same window");
        m_modalNestedLevel++;
        return;
    }
    
    m_modalWindow = modalWindow;
    m_modalNestedLevel = 1;
    
    if ( modalWindow )
    {
        // we must show now, otherwise beginModalSessionForWindow does it but it
        // also would do a centering of the window before overriding all our position
        if ( !modalWindow->IsShownOnScreen() )
            modalWindow->Show();
        
        wxNonOwnedWindow* now = dynamic_cast<wxNonOwnedWindow*> (modalWindow);
        wxASSERT_MSG( now != NULL, "must pass in a toplevel window for modal event loop" );
        nsnow = now ? now->GetWXWindow() : nil;
    }
    else
    {
        NSRect r = NSMakeRect(10, 10, 0, 0);
        nsnow = [NSPanel alloc];
        [nsnow initWithContentRect:r
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:YES
         ];
        [nsnow orderOut:nil];
        m_dummyWindow = nsnow;
    }
    m_modalSession = [NSApp beginModalSessionForWindow:nsnow];
    wxASSERT_MSG(m_modalSession != NULL, "modal session couldn't be started");
}

void wxGUIEventLoop::EndModalSession()
{
    wxASSERT_MSG(m_modalSession != NULL, "no modal session active");
    
    wxASSERT_MSG(m_modalNestedLevel > 0, "incorrect modal nesting level");
    
    if ( --m_modalNestedLevel == 0 )
    {
        [NSApp endModalSession:(NSModalSession)m_modalSession];
        m_modalSession = nil;
        if ( m_dummyWindow )
        {
            [m_dummyWindow release];
            m_dummyWindow = nil;
        }
    }
}

//
// 
//

wxWindowDisabler::wxWindowDisabler(bool disable)
{
    m_modalEventLoop = NULL;
    m_disabled = disable;
    if ( disable )
        DoDisable();
}

wxWindowDisabler::wxWindowDisabler(wxWindow *winToSkip)
{
    m_disabled = true;
    DoDisable(winToSkip);
}

void wxWindowDisabler::DoDisable(wxWindow *winToSkip)
{    
    // remember the top level windows which were already disabled, so that we
    // don't reenable them later
    m_winDisabled = NULL;
    
    wxWindowList::compatibility_iterator node;
    for ( node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext() )
    {
        wxWindow *winTop = node->GetData();
        if ( winTop == winToSkip )
            continue;
        
        // we don't need to disable the hidden or already disabled windows
        if ( winTop->IsEnabled() && winTop->IsShown() )
        {
            winTop->Disable();
        }
        else
        {
            if ( !m_winDisabled )
            {
                m_winDisabled = new wxWindowList;
            }
            
            m_winDisabled->Append(winTop);
        }
    }
    
    m_modalEventLoop = (wxEventLoop*)wxEventLoopBase::GetActive();
    if (m_modalEventLoop)
        m_modalEventLoop->BeginModalSession(winToSkip);
}

wxWindowDisabler::~wxWindowDisabler()
{
    if ( !m_disabled )
        return;
    
    if (m_modalEventLoop)
        m_modalEventLoop->EndModalSession();
    
    wxWindowList::compatibility_iterator node;
    for ( node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext() )
    {
        wxWindow *winTop = node->GetData();
        if ( !m_winDisabled || !m_winDisabled->Find(winTop) )
        {
            winTop->Enable();
        }
        //else: had been already disabled, don't reenable
    }
    
    delete m_winDisabled;
}