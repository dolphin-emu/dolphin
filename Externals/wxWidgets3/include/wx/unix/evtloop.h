///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/evtloop.h
// Purpose:     declares wxEventLoop class
// Author:      Lukasz Michalski (lm@zork.pl)
// Created:     2007-05-07
// RCS-ID:      $Id: evtloop.h 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) 2007 Lukasz Michalski
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_EVTLOOP_H_
#define _WX_UNIX_EVTLOOP_H_

#if wxUSE_CONSOLE_EVENTLOOP

// ----------------------------------------------------------------------------
// wxConsoleEventLoop
// ----------------------------------------------------------------------------

class wxFDIODispatcher;
class wxUnixEventLoopSource;

namespace wxPrivate
{
    class PipeIOHandler;
}

class WXDLLIMPEXP_BASE wxConsoleEventLoop
#ifdef __WXOSX__
: public wxCFEventLoop
#else
: public wxEventLoopManual
#endif
{
public:
    // initialize the event loop, use IsOk() to check if we were successful
    wxConsoleEventLoop();
    virtual ~wxConsoleEventLoop();

    // implement base class pure virtuals
    virtual bool Pending() const;
    virtual bool Dispatch();
    virtual int DispatchTimeout(unsigned long timeout);
    virtual void WakeUp();
    virtual bool IsOk() const { return m_dispatcher != NULL; }
    virtual bool YieldFor(long WXUNUSED(eventsToProcess)) { return true; }

#if wxUSE_EVENTLOOP_SOURCE
    virtual wxEventLoopSource *
      AddSourceForFD(int fd, wxEventLoopSourceHandler *handler, int flags);
#endif // wxUSE_EVENTLOOP_SOURCE

protected:
    virtual void OnNextIteration();

private:
    // pipe used for wake up messages: when a child thread wants to wake up
    // the event loop in the main thread it writes to this pipe
    wxPrivate::PipeIOHandler *m_wakeupPipe;

    // either wxSelectDispatcher or wxEpollDispatcher
    wxFDIODispatcher *m_dispatcher;

    wxDECLARE_NO_COPY_CLASS(wxConsoleEventLoop);
};

#endif // wxUSE_CONSOLE_EVENTLOOP

#endif // _WX_UNIX_EVTLOOP_H_
