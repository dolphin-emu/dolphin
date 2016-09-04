///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/evtloop.mm
// Purpose:     implementation of wxEventLoop for OS X
// Author:      Vadim Zeitlin, Stefan Csomor
// Modified by:
// Created:     2006-01-12
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
#include "wx/scopeguard.h"
#include "wx/vector.h"
#include "wx/hashmap.h"

#include "wx/osx/private.h"

struct wxModalSessionStackElement
{
    WXWindow        dummyWindow;
    void*           modalSession;
};

typedef wxVector<wxModalSessionStackElement> wxModalSessionStack;

WX_DECLARE_HASH_MAP(wxGUIEventLoop*, wxModalSessionStack*, wxPointerHash, wxPointerEqual,
                    wxModalSessionStackMap);

static wxModalSessionStackMap gs_modalSessionStackMap;

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
            NSTabletPointMask |
            NSTabletProximityMask |
            NSOtherMouseDownMask |
            NSOtherMouseUpMask |
            NSOtherMouseDraggedMask |

            NSKeyDownMask |
            NSKeyUpMask |
            NSFlagsChangedMask |
            NSEventMaskGesture |
            NSEventMaskMagnify |
            NSEventMaskSwipe |
            NSEventMaskRotate |
            NSEventMaskBeginGesture |
            NSEventMaskEndGesture |
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
    m_osxLowLevelWakeUp = false;
}

wxGUIEventLoop::~wxGUIEventLoop()
{
    wxASSERT( gs_modalSessionStackMap.find( this ) == gs_modalSessionStackMap.end() );
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
                        untilDate: [NSDate dateWithTimeIntervalSinceNow: timeout/1000.0]
                        inMode: NSDefaultRunLoopMode
                        dequeue: NO] != nil )
                    return 1;
                
                return -1;
            }
            case NSRunStoppedResponse:
            case NSRunAbortedResponse:
                return -1;
            default:
                // nested native loops may return other codes here, just ignore them
                break;
        }
        return -1;
    }
    else 
    {        
        NSEvent *event = [NSApp
                    nextEventMatchingMask:NSAnyEventMask
                    untilDate:[NSDate dateWithTimeIntervalSinceNow: timeout/1000.0]
                    inMode:NSDefaultRunLoopMode
                    dequeue: YES];
        
        if ( event == nil )
            return -1;

        [NSApp sendEvent: event];

        return 1;
    }
}

static int gs_loopNestingLevel = 0;

void wxGUIEventLoop::OSXDoRun()
{
    /*
       In order to properly nest GUI event loops in Cocoa, it is important to
       have [NSApp run] only as the main/outermost event loop.  There are many
       problems if [NSApp run] is used as an inner event loop.  The main issue
       is that a call to [NSApp stop] is needed to exit an [NSApp run] event
       loop. But the [NSApp stop] has some side effects that we do not want -
       such as if there was a modal dialog box with a modal event loop running,
       that event loop would also get exited, and the dialog would be closed.
       The call to [NSApp stop] would also cause the enclosing event loop to
       exit as well.

       webkit's webcore library uses CFRunLoopRun() for nested event loops. See
       the log of the commit log about the change in webkit's webcore module:
       http://www.mail-archive.com/webkit-changes@lists.webkit.org/msg07397.html

       See here for the latest run loop that is used in webcore:
       https://github.com/WebKit/webkit/blob/master/Source/WebCore/platform/mac/RunLoopMac.mm

       CFRunLoopRun() was tried for the nested event loop here but it causes a
       problem in that all user input is disabled - and there is no way to
       re-enable it.  The caller of this event loop may not want user input
       disabled (such as synchronous wxExecute with wxEXEC_NODISABLE flag).

       In order to have an inner event loop where user input can be enabled,
       the old wxCocoa code that used the [NSApp nextEventMatchingMask] was
       borrowed but changed to use blocking instead of polling. By specifying
       'distantFuture' in 'untildate', we can have it block until the next
       event.  Then we can keep looping on each new event until m_shouldExit is
       raised to exit the event loop.
    */
    gs_loopNestingLevel++;
    wxON_BLOCK_EXIT_SET(gs_loopNestingLevel, gs_loopNestingLevel - 1);

    while ( !m_shouldExit )
    {
        // By putting this inside the loop, we can drain it in each
        // loop iteration.
        wxMacAutoreleasePool autoreleasepool;

        if ( gs_loopNestingLevel == 1 )
        {
            // Use -[NSApplication run] for the main run loop.
            [NSApp run];
        }
        else
        {
            // We use this blocking call to [NSApp nextEventMatchingMask:...]
            // because the other methods (such as CFRunLoopRun() and [runLoop
            // runMode:beforeDate] were always disabling input to the windows
            // (even if we wanted it enabled).
            //
            // Here are the other run loops which were tried, but always left
            // user input disabled:
            //
            // [runLoop runMode:NSDefaultRunLoopMode beforeDate:date];
            // CFRunLoopRun();
            // CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10 , true);
            //
            // Using [NSApp nextEventMatchingMask:...] would leave windows
            // enabled if we wanted them to be, so that is why it is used.
            NSEvent *event = [NSApp
                    nextEventMatchingMask:NSAnyEventMask
                    untilDate:[NSDate distantFuture]
                    inMode:NSDefaultRunLoopMode
                    dequeue: YES];

            [NSApp sendEvent: event];

            /**
              The NSApplication documentation states that:

              "
              This method is invoked automatically in the main event loop
              after each event when running in NSDefaultRunLoopMode or
              NSModalRunLoopMode. This method is not invoked automatically
              when running in NSEventTrackingRunLoopMode.
              "

              So to be safe, we also invoke it here in this event loop.

              See: https://developer.apple.com/library/mac/#documentation/Cocoa/Reference/ApplicationKit/Classes/NSApplication_Class/Reference/Reference.html
            */
            [NSApp updateWindows];
        }
    }

    // Wake up the enclosing loop so that it can check if it also needs
    // to exit.
    WakeUp();
}

void wxGUIEventLoop::OSXDoStop()
{
    // We should only stop the top level event loop.
    if ( gs_loopNestingLevel <= 1 )
    {
        [NSApp stop:0];
    }

    // For the top level loop only calling stop: is not enough when called from
    // a runloop-observer, therefore add a dummy event, to make sure the
    // runloop gets another round. And for the nested loops we need to wake it
    // up to notice that it should exit, so do this unconditionally.
    WakeUp();
}

void wxGUIEventLoop::WakeUp()
{
    // NSEvent* cevent = [NSApp currentEvent];
    // NSString* mode = [[NSRunLoop mainRunLoop] currentMode];
    
    // when already in a mouse event handler, don't add higher level event
    // if ( cevent != nil && [cevent type] <= NSMouseMoved && )
    if ( m_osxLowLevelWakeUp /* [NSEventTrackingRunLoopMode isEqualToString:mode] */ )
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

#define OSX_USE_MODAL_SESSION 1

void wxModalEventLoop::OSXDoRun()
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
#if OSX_USE_MODAL_SESSION
    if ( m_modalWindow )
    {
        BeginModalSession(m_modalWindow);
        wxCFEventLoop::OSXDoRun();
        EndModalSession();
    }
    else
#endif
    {
        [NSApp runModalForWindow:m_modalNativeWindow];
    }
}

void wxModalEventLoop::OSXDoStop()
{
    [NSApp abortModal];
}

void wxGUIEventLoop::BeginModalSession( wxWindow* modalWindow )
{
    WXWindow nsnow = nil;

    m_modalNestedLevel++;
    if ( m_modalNestedLevel > 1 )
    {
        wxModalSessionStack* stack = NULL;
        
        if ( m_modalNestedLevel == 2 )
        {
            stack = new wxModalSessionStack;
            gs_modalSessionStackMap[this] = stack;
        }
        else
        {
            stack = gs_modalSessionStackMap[this];
        }
        
        wxModalSessionStackElement element;
        element.dummyWindow = m_dummyWindow;
        element.modalSession = m_modalSession;

        stack->push_back(element);

        // shortcut if nothing changed in this level

        if ( m_modalWindow == modalWindow )
            return;
    }
    
    m_modalWindow = modalWindow;
    
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
        m_dummyWindow = nsnow;
        [nsnow orderOut:nil];
    }
    m_modalSession = [NSApp beginModalSessionForWindow:nsnow];
    wxASSERT_MSG(m_modalSession != NULL, "modal session couldn't be started");
}

void wxGUIEventLoop::EndModalSession()
{
    wxASSERT_MSG(m_modalSession != NULL, "no modal session active");
    
    wxASSERT_MSG(m_modalNestedLevel > 0, "incorrect modal nesting level");
    
    --m_modalNestedLevel;
    if ( m_modalNestedLevel == 0 )
    {
        [NSApp endModalSession:(NSModalSession)m_modalSession];
        m_modalSession = nil;
        if ( m_dummyWindow )
        {
            [m_dummyWindow release];
            m_dummyWindow = nil;
        }
    }
    else
    {
        wxModalSessionStack* stack = gs_modalSessionStackMap[this];
        wxModalSessionStackElement element = stack->back();
        stack->pop_back();

        if( m_modalNestedLevel == 1 )
        {
            gs_modalSessionStackMap.erase(this);
            delete stack;
        }

        if ( m_modalSession != element.modalSession )
        {
            [NSApp endModalSession:(NSModalSession)m_modalSession];
            m_modalSession = element.modalSession;
        }

        if ( m_dummyWindow != element.dummyWindow )
        {
            if ( element.dummyWindow )
                [element.dummyWindow release];

            m_dummyWindow = element.dummyWindow;
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
