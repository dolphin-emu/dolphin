///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/evtloop_cf.cpp
// Purpose:     wxEventLoop implementation common to both Carbon and Cocoa
// Author:      Vadim Zeitlin
// Created:     2009-10-18
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
//              (c) 2013 Rob Bresalier
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
    #include "wx/log.h"
    #include "wx/app.h"
#endif

#include "wx/evtloopsrc.h"

#include "wx/scopedptr.h"

#include "wx/osx/private.h"
#include "wx/osx/core/cfref.h"
#include "wx/thread.h"

#if wxUSE_GUI
    #include "wx/nonownedwnd.h"
#endif

#include <CoreFoundation/CFSocket.h>

// ============================================================================
// wxCFEventLoopSource and wxCFEventLoop implementation
// ============================================================================

#if wxUSE_EVENTLOOP_SOURCE

void wxCFEventLoopSource::InitSourceSocket(CFSocketRef cfSocket)
{
    wxASSERT_MSG( !m_cfSocket, "shouldn't be called more than once" );

    m_cfSocket = cfSocket;
}

wxCFEventLoopSource::~wxCFEventLoopSource()
{
    if ( m_cfSocket )
    {
        CFSocketInvalidate(m_cfSocket);
        CFRelease(m_cfSocket);
    }
}

#endif // wxUSE_EVENTLOOP_SOURCE

void wxCFEventLoop::OSXCommonModeObserverCallBack(CFRunLoopObserverRef observer, int activity, void *info)
{
    wxCFEventLoop * eventloop = static_cast<wxCFEventLoop *>(info);
    if ( eventloop && eventloop->IsRunning() )
        eventloop->CommonModeObserverCallBack(observer, activity);
}

void wxCFEventLoop::OSXDefaultModeObserverCallBack(CFRunLoopObserverRef observer, int activity, void *info)
{
    wxCFEventLoop * eventloop = static_cast<wxCFEventLoop *>(info);
    if ( eventloop && eventloop->IsRunning() )
        eventloop->DefaultModeObserverCallBack(observer, activity);
}

void wxCFEventLoop::CommonModeObserverCallBack(CFRunLoopObserverRef WXUNUSED(observer), int activity)
{
    if ( activity & kCFRunLoopBeforeTimers )
    {
        // process pending wx events first as they correspond to low-level events
        // which happened before, i.e. typically pending events were queued by a
        // previous call to Dispatch() and if we didn't process them now the next
        // call to it might enqueue them again (as happens with e.g. socket events
        // which would be generated as long as there is input available on socket
        // and this input is only removed from it when pending event handlers are
        // executed)

        if ( wxTheApp && ShouldProcessIdleEvents() )
            wxTheApp->ProcessPendingEvents();
    }

    if ( activity & kCFRunLoopBeforeWaiting )
    {
        if ( ShouldProcessIdleEvents() && ProcessIdle() )
        {
            WakeUp();
        }
        else
        {
#if wxUSE_THREADS
            wxMutexGuiLeaveOrEnter();
#endif
        }
    }
}

void
wxCFEventLoop::DefaultModeObserverCallBack(CFRunLoopObserverRef WXUNUSED(observer),
                                           int WXUNUSED(activity))
{
    /*
    if ( activity & kCFRunLoopBeforeTimers )
    {
    }
    
    if ( activity & kCFRunLoopBeforeWaiting )
    {
    }
    */
}


wxCFEventLoop::wxCFEventLoop()
{
    m_shouldExit = false;
    m_processIdleEvents = true;

#if wxUSE_UIACTIONSIMULATOR
    m_shouldWaitForEvent = false;
#endif
    
    m_runLoop = CFGetCurrentRunLoop();

    CFRunLoopObserverContext ctxt;
    bzero( &ctxt, sizeof(ctxt) );
    ctxt.info = this;
    m_commonModeRunLoopObserver = CFRunLoopObserverCreate( kCFAllocatorDefault, kCFRunLoopBeforeTimers | kCFRunLoopBeforeWaiting , true /* repeats */, 0,
                                                          (CFRunLoopObserverCallBack) wxCFEventLoop::OSXCommonModeObserverCallBack, &ctxt );
    CFRunLoopAddObserver(m_runLoop, m_commonModeRunLoopObserver, kCFRunLoopCommonModes);

    m_defaultModeRunLoopObserver = CFRunLoopObserverCreate( kCFAllocatorDefault, kCFRunLoopBeforeTimers | kCFRunLoopBeforeWaiting , true /* repeats */, 0,
                                                           (CFRunLoopObserverCallBack) wxCFEventLoop::OSXDefaultModeObserverCallBack, &ctxt );
    CFRunLoopAddObserver(m_runLoop, m_defaultModeRunLoopObserver, kCFRunLoopDefaultMode);
}

wxCFEventLoop::~wxCFEventLoop()
{
    CFRunLoopRemoveObserver(m_runLoop, m_commonModeRunLoopObserver, kCFRunLoopCommonModes);
    CFRunLoopRemoveObserver(m_runLoop, m_defaultModeRunLoopObserver, kCFRunLoopDefaultMode);

    CFRelease(m_defaultModeRunLoopObserver);
    CFRelease(m_commonModeRunLoopObserver);
}


CFRunLoopRef wxCFEventLoop::CFGetCurrentRunLoop() const
{
    return CFRunLoopGetCurrent();
}

void wxCFEventLoop::WakeUp()
{
    CFRunLoopWakeUp(m_runLoop);
}

#if wxUSE_BASE

void wxMacWakeUp()
{
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    if ( loop )
        loop->WakeUp();
}

#endif

void wxCFEventLoop::DoYieldFor(long eventsToProcess)
{
    // process all pending events:
    while ( DoProcessEvents() == 1 )
        ;

    wxEventLoopBase::DoYieldFor(eventsToProcess);
}

// implement/override base class pure virtual
bool wxCFEventLoop::Pending() const
{
    return true;
}

int wxCFEventLoop::DoProcessEvents()
{
#if wxUSE_UIACTIONSIMULATOR
    if ( m_shouldWaitForEvent )
    {
        int  handled = DispatchTimeout( 1000 );
        wxASSERT_MSG( handled == 1, "No Event Available");
        m_shouldWaitForEvent = false;
        return handled;
    }
    else
#endif
        return DispatchTimeout( IsYielding() ? 0 : 1000 );
}

bool wxCFEventLoop::Dispatch()
{
    return DoProcessEvents() != 0;
}

int wxCFEventLoop::DispatchTimeout(unsigned long timeout)
{
    if ( !wxTheApp )
        return 0;

    int status = DoDispatchTimeout(timeout);

    switch( status )
    {
        case 0:
            break;
        case -1:
            if ( m_shouldExit )
                return 0;

            break;
        case 1:
            break;
    }

    return status;
}

int wxCFEventLoop::DoDispatchTimeout(unsigned long timeout)
{
    SInt32 status = CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout / 1000.0 , true);
    switch( status )
    {
        case kCFRunLoopRunFinished:
            wxFAIL_MSG( "incorrect run loop state" );
            break;
        case kCFRunLoopRunStopped:
            return 0;
            break;
        case kCFRunLoopRunTimedOut:
            return -1;
            break;
        case kCFRunLoopRunHandledSource:
        default:
            break;
    }
    return 1;
}

void wxCFEventLoop::OSXDoRun()
{
    for ( ;; )
    {
        OnNextIteration();

        // generate and process idle events for as long as we don't
        // have anything else to do
        DoProcessEvents();

        // if the "should exit" flag is set, the loop should terminate
        // but not before processing any remaining messages so while
        // Pending() returns true, do process them
        if ( m_shouldExit )
        {
            while ( DoProcessEvents() == 1 )
                ;

            break;
        }
    }
}

void wxCFEventLoop::OSXDoStop()
{
    CFRunLoopStop(CFGetCurrentRunLoop());
}

// enters a loop calling OnNextIteration(), Pending() and Dispatch() and
// terminating when Exit() is called
int wxCFEventLoop::DoRun()
{
    // we must ensure that OnExit() is called even if an exception is thrown
    // from inside ProcessEvents() but we must call it from Exit() in normal
    // situations because it is supposed to be called synchronously,
    // wxModalEventLoop depends on this (so we can't just use ON_BLOCK_EXIT or
    // something similar here)
#if wxUSE_EXCEPTIONS
    for ( ;; )
    {
        try
        {
#endif // wxUSE_EXCEPTIONS

            OSXDoRun();

#if wxUSE_EXCEPTIONS
            // exit the outer loop as well
            break;
        }
        catch ( ... )
        {
            try
            {
                if ( !wxTheApp || !wxTheApp->OnExceptionInMainLoop() )
                {
                    OnExit();
                    break;
                }
                //else: continue running the event loop
            }
            catch ( ... )
            {
                // OnException() throwed, possibly rethrowing the same
                // exception again: very good, but we still need OnExit() to
                // be called
                OnExit();
                throw;
            }
        }
    }
#endif // wxUSE_EXCEPTIONS

    return m_exitcode;
}

// sets the "should exit" flag and wakes up the loop so that it terminates
// soon
void wxCFEventLoop::ScheduleExit(int rc)
{
    m_exitcode = rc;
    m_shouldExit = true;
    OSXDoStop();
}

wxCFEventLoopPauseIdleEvents::wxCFEventLoopPauseIdleEvents()
{
    wxCFEventLoop* cfl = dynamic_cast<wxCFEventLoop*>(wxEventLoopBase::GetActive());
    if ( cfl )
    {
        m_formerState = cfl->ShouldProcessIdleEvents();
        cfl->SetProcessIdleEvents(false);
    }
    else
        m_formerState = true;
}

wxCFEventLoopPauseIdleEvents::~wxCFEventLoopPauseIdleEvents()
{
    wxCFEventLoop* cfl = dynamic_cast<wxCFEventLoop*>(wxEventLoopBase::GetActive());
    if ( cfl )
        cfl->SetProcessIdleEvents(m_formerState);
}

// TODO Move to thread_osx.cpp

#if wxUSE_THREADS

// ----------------------------------------------------------------------------
// GUI Serialization copied from MSW implementation
// ----------------------------------------------------------------------------

// if it's false, some secondary thread is holding the GUI lock
static bool gs_bGuiOwnedByMainThread = true;

// critical section which controls access to all GUI functions: any secondary
// thread (i.e. except the main one) must enter this crit section before doing
// any GUI calls
static wxCriticalSection *gs_critsectGui = NULL;

// critical section which protects gs_nWaitingForGui variable
static wxCriticalSection *gs_critsectWaitingForGui = NULL;

// number of threads waiting for GUI in wxMutexGuiEnter()
static size_t gs_nWaitingForGui = 0;

void wxOSXThreadModuleOnInit()
{
    gs_critsectWaitingForGui = new wxCriticalSection();
    gs_critsectGui = new wxCriticalSection();
    gs_critsectGui->Enter();
}


void wxOSXThreadModuleOnExit()
{
    if ( gs_critsectGui )
    {
        if ( !wxGuiOwnedByMainThread() )
        {
            gs_critsectGui->Enter();
            gs_bGuiOwnedByMainThread = true;
        }

        gs_critsectGui->Leave();
        wxDELETE(gs_critsectGui);
    }

    wxDELETE(gs_critsectWaitingForGui);
}


// wake up the main thread
void WXDLLIMPEXP_BASE wxWakeUpMainThread()
{
    wxMacWakeUp();
}

void wxMutexGuiEnterImpl()
{
    // this would dead lock everything...
    wxASSERT_MSG( !wxThread::IsMain(),
                 wxT("main thread doesn't want to block in wxMutexGuiEnter()!") );

    // the order in which we enter the critical sections here is crucial!!

    // set the flag telling to the main thread that we want to do some GUI
    {
        wxCriticalSectionLocker enter(*gs_critsectWaitingForGui);

        gs_nWaitingForGui++;
    }

    wxWakeUpMainThread();

    // now we may block here because the main thread will soon let us in
    // (during the next iteration of OnIdle())
    gs_critsectGui->Enter();
}

void wxMutexGuiLeaveImpl()
{
    wxCriticalSectionLocker enter(*gs_critsectWaitingForGui);

    if ( wxThread::IsMain() )
    {
        gs_bGuiOwnedByMainThread = false;
    }
    else
    {
        // decrement the number of threads waiting for GUI access now
        wxASSERT_MSG( gs_nWaitingForGui > 0,
                     wxT("calling wxMutexGuiLeave() without entering it first?") );

        gs_nWaitingForGui--;

        wxWakeUpMainThread();
    }

    gs_critsectGui->Leave();
}

void WXDLLIMPEXP_BASE wxMutexGuiLeaveOrEnter()
{
    wxASSERT_MSG( wxThread::IsMain(),
                 wxT("only main thread may call wxMutexGuiLeaveOrEnter()!") );

    if ( !gs_critsectWaitingForGui )
        return;

    wxCriticalSectionLocker enter(*gs_critsectWaitingForGui);

    if ( gs_nWaitingForGui == 0 )
    {
        // no threads are waiting for GUI - so we may acquire the lock without
        // any danger (but only if we don't already have it)
        if ( !wxGuiOwnedByMainThread() )
        {
            gs_critsectGui->Enter();

            gs_bGuiOwnedByMainThread = true;
        }
        //else: already have it, nothing to do
    }
    else
    {
        // some threads are waiting, release the GUI lock if we have it
        if ( wxGuiOwnedByMainThread() )
            wxMutexGuiLeave();
        //else: some other worker thread is doing GUI
    }
}

bool WXDLLIMPEXP_BASE wxGuiOwnedByMainThread()
{
    return gs_bGuiOwnedByMainThread;
}

#endif
