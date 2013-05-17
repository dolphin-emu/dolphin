///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/evtloop.h
// Purpose:     simply forwards to wx/osx/carbon/evtloop.h or
//              wx/osx/cocoa/evtloop.h for consistency with the other Mac
//              headers
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2006-01-12
// RCS-ID:      $Id: evtloop.h 67724 2011-05-11 06:46:07Z SC $
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_EVTLOOP_H_
#define _WX_OSX_EVTLOOP_H_

DECLARE_WXOSX_OPAQUE_CFREF( CFRunLoop );
DECLARE_WXOSX_OPAQUE_CFREF( CFRunLoopObserver );

class WXDLLIMPEXP_BASE wxCFEventLoop : public wxEventLoopBase
{
public:
    wxCFEventLoop();
    virtual ~wxCFEventLoop();

    // enters a loop calling OnNextIteration(), Pending() and Dispatch() and
    // terminating when Exit() is called
    virtual int Run();

    // sets the "should exit" flag and wakes up the loop so that it terminates
    // soon
    virtual void Exit(int rc = 0);

    // return true if any events are available
    virtual bool Pending() const;

    // dispatch a single event, return false if we should exit from the loop
    virtual bool Dispatch();

    // same as Dispatch() but doesn't wait for longer than the specified (in
    // ms) timeout, return true if an event was processed, false if we should
    // exit the loop or -1 if timeout expired
    virtual int DispatchTimeout(unsigned long timeout);

    // implement this to wake up the loop: usually done by posting a dummy event
    // to it (can be called from non main thread)
    virtual void WakeUp();

    virtual bool YieldFor(long eventsToProcess);

#if wxUSE_EVENTLOOP_SOURCE
    virtual wxEventLoopSource *
      AddSourceForFD(int fd, wxEventLoopSourceHandler *handler, int flags);
#endif // wxUSE_EVENTLOOP_SOURCE


protected:
    void CommonModeObserverCallBack(CFRunLoopObserverRef observer, int activity);
    void DefaultModeObserverCallBack(CFRunLoopObserverRef observer, int activity);

    static void OSXCommonModeObserverCallBack(CFRunLoopObserverRef observer, int activity, void *info);
    static void OSXDefaultModeObserverCallBack(CFRunLoopObserverRef observer, int activity, void *info);

    // get the currently executing CFRunLoop
    virtual CFRunLoopRef CFGetCurrentRunLoop() const;

    virtual int DoDispatchTimeout(unsigned long timeout);

    virtual void DoRun();

    virtual void DoStop();

    // should we exit the loop?
    bool m_shouldExit;

    // the loop exit code
    int m_exitcode;

    // cfrunloop
    CFRunLoopRef m_runLoop;

    // common modes runloop observer
    CFRunLoopObserverRef m_commonModeRunLoopObserver;

    // default mode runloop observer
    CFRunLoopObserverRef m_defaultModeRunLoopObserver;

private:
    // process all already pending events and dispatch a new one (blocking
    // until it appears in the event queue if necessary)
    //
    // returns the return value of DoDispatchTimeout()
    int DoProcessEvents();
};

#if wxUSE_GUI

#ifdef __WXOSX_COCOA__
    #include "wx/osx/cocoa/evtloop.h"
#else
    #include "wx/osx/carbon/evtloop.h"
#endif

class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxNonOwnedWindow;

class WXDLLIMPEXP_CORE wxModalEventLoop : public wxGUIEventLoop
{
public:
    wxModalEventLoop(wxWindow *modalWindow);
    wxModalEventLoop(WXWindow modalNativeWindow);

protected:
    virtual void DoRun();

    virtual void DoStop();

    // (in case) the modal window for this event loop
    wxNonOwnedWindow* m_modalWindow;
    WXWindow m_modalNativeWindow;
};

#endif // wxUSE_GUI

#endif // _WX_OSX_EVTLOOP_H_
