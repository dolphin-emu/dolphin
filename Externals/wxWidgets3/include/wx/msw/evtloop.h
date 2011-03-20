///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/evtloop.h
// Purpose:     wxEventLoop class for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-07-31
// RCS-ID:      $Id: evtloop.h 59161 2009-02-26 14:15:20Z VZ $
// Copyright:   (c) 2003-2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_EVTLOOP_H_
#define _WX_MSW_EVTLOOP_H_

#if wxUSE_GUI
#include "wx/dynarray.h"
#include "wx/msw/wrapwin.h"
#include "wx/window.h"
#endif

// ----------------------------------------------------------------------------
// wxEventLoop
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMSWEventLoopBase : public wxEventLoopManual
{
public:
    wxMSWEventLoopBase();

    // implement base class pure virtuals
    virtual bool Pending() const;

protected:
    // get the next message from queue and return true or return false if we
    // got WM_QUIT or an error occurred
    bool GetNextMessage(WXMSG *msg);

    // same as above but with a timeout and return value can be -1 meaning that
    // time out expired in addition to
    int GetNextMessageTimeout(WXMSG *msg, unsigned long timeout);
};

#if wxUSE_GUI

WX_DECLARE_EXPORTED_OBJARRAY(MSG, wxMSGArray);

class WXDLLIMPEXP_CORE wxGUIEventLoop : public wxMSWEventLoopBase
{
public:
    wxGUIEventLoop() { }

    // process a single message: calls PreProcessMessage() before dispatching
    // it
    virtual void ProcessMessage(WXMSG *msg);

    // preprocess a message, return true if processed (i.e. no further
    // dispatching required)
    virtual bool PreProcessMessage(WXMSG *msg);

    // set the critical window: this is the window such that all the events
    // except those to this window (and its children) stop to be processed
    // (typical examples: assert or crash report dialog)
    //
    // calling this function with NULL argument restores the normal event
    // handling
    static void SetCriticalWindow(wxWindowMSW *win) { ms_winCritical = win; }

    // return true if there is no critical window or if this window is [a child
    // of] the critical one
    static bool AllowProcessing(wxWindowMSW *win)
    {
        return !ms_winCritical || IsChildOfCriticalWindow(win);
    }

    // override/implement base class virtuals
    virtual bool Dispatch();
    virtual int DispatchTimeout(unsigned long timeout);
    virtual void WakeUp();
    virtual bool YieldFor(long eventsToProcess);

protected:
    virtual void OnNextIteration();

private:
    // check if the given window is a child of ms_winCritical (which must be
    // non NULL)
    static bool IsChildOfCriticalWindow(wxWindowMSW *win);

    // array of messages used for temporary storage by YieldFor()
    wxMSGArray m_arrMSG;

    // critical window or NULL
    static wxWindowMSW *ms_winCritical;
};

#else // !wxUSE_GUI

#if wxUSE_CONSOLE_EVENTLOOP

class WXDLLIMPEXP_BASE wxConsoleEventLoop : public wxMSWEventLoopBase
{
public:
    wxConsoleEventLoop() { }

    // override/implement base class virtuals
    virtual bool Dispatch();
    virtual int DispatchTimeout(unsigned long timeout);
    virtual void WakeUp();
    virtual bool YieldFor(long WXUNUSED(eventsToProcess)) { return true; }

    // MSW-specific function to process a single message
    virtual void ProcessMessage(WXMSG *msg);
};

#endif // wxUSE_CONSOLE_EVENTLOOP

#endif // wxUSE_GUI/!wxUSE_GUI

#endif // _WX_MSW_EVTLOOP_H_
