/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/evtloopunix.cpp
// Purpose:     wxEventLoop implementation
// Author:      Lukasz Michalski (lm@zork.pl)
// Created:     2007-05-07
// RCS-ID:      $Id: evtloopunix.cpp 65992 2010-11-02 11:57:19Z VZ $
// Copyright:   (c) 2006 Zork Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_CONSOLE_EVENTLOOP

#include "wx/evtloop.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
#endif

#include <errno.h>
#include "wx/apptrait.h"
#include "wx/scopedptr.h"
#include "wx/thread.h"
#include "wx/module.h"
#include "wx/unix/pipe.h"
#include "wx/unix/private/timer.h"
#include "wx/unix/private/epolldispatcher.h"
#include "wx/private/selectdispatcher.h"

#if wxUSE_EVENTLOOP_SOURCE
    #include "wx/evtloopsrc.h"
#endif // wxUSE_EVENTLOOP_SOURCE

#define TRACE_EVENTS wxT("events")

// ===========================================================================
// wxEventLoop::PipeIOHandler implementation
// ===========================================================================

namespace wxPrivate
{

// pipe used for wake up messages: when a child thread wants to wake up
// the event loop in the main thread it writes to this pipe
class PipeIOHandler : public wxFDIOHandler
{
public:
    // default ctor does nothing, call Create() to really initialize the
    // object
    PipeIOHandler() { }

    bool Create();

    // this method can be, and normally is, called from another thread
    void WakeUp();

    int GetReadFd() { return m_pipe[wxPipe::Read]; }

    // implement wxFDIOHandler pure virtual methods
    virtual void OnReadWaiting();
    virtual void OnWriteWaiting() { }
    virtual void OnExceptionWaiting() { }

private:
    wxPipe m_pipe;
};

// ----------------------------------------------------------------------------
// initialization
// ----------------------------------------------------------------------------

bool PipeIOHandler::Create()
{
    if ( !m_pipe.Create() )
    {
        wxLogError(_("Failed to create wake up pipe used by event loop."));
        return false;
    }

    if ( !m_pipe.MakeNonBlocking(wxPipe::Read) )
    {
        wxLogSysError(_("Failed to switch wake up pipe to non-blocking mode"));
        return false;
    }

    wxLogTrace(TRACE_EVENTS, wxT("Wake up pipe (%d, %d) created"),
               m_pipe[wxPipe::Read], m_pipe[wxPipe::Write]);

    return true;
}

// ----------------------------------------------------------------------------
// wakeup handling
// ----------------------------------------------------------------------------

void PipeIOHandler::WakeUp()
{
    if ( write(m_pipe[wxPipe::Write], "s", 1) != 1 )
    {
        // don't use wxLog here, we can be in another thread and this could
        // result in dead locks
        perror("write(wake up pipe)");
    }
}

void PipeIOHandler::OnReadWaiting()
{
    // got wakeup from child thread: read all data available in pipe just to
    // make it empty (even though we write one byte at a time from WakeUp(),
    // it could have been called several times)
    char buf[4];
    for ( ;; )
    {
        const int size = read(GetReadFd(), buf, WXSIZEOF(buf));

        if ( size == 0 || (size == -1 && (errno == EAGAIN || errno == EINTR)) )
        {
            // nothing left in the pipe (EAGAIN is expected for an FD with
            // O_NONBLOCK)
            break;
        }

        if ( size == -1 )
        {
            wxLogSysError(_("Failed to read from wake-up pipe"));

            break;
        }
    }

    // writing to the wake up pipe will make wxConsoleEventLoop return from
    // wxFDIODispatcher::Dispatch() it might be currently blocking in, nothing
    // else needs to be done
}

} // namespace wxPrivate

// ===========================================================================
// wxEventLoop implementation
// ===========================================================================

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------

wxConsoleEventLoop::wxConsoleEventLoop()
{
    m_wakeupPipe = new wxPrivate::PipeIOHandler();
    if ( !m_wakeupPipe->Create() )
    {
        wxDELETE(m_wakeupPipe);
        m_dispatcher = NULL;
        return;
    }

    m_dispatcher = wxFDIODispatcher::Get();
    if ( !m_dispatcher )
        return;

    m_dispatcher->RegisterFD
                  (
                    m_wakeupPipe->GetReadFd(),
                    m_wakeupPipe,
                    wxFDIO_INPUT
                  );
}

wxConsoleEventLoop::~wxConsoleEventLoop()
{
    if ( m_wakeupPipe )
    {
        if ( m_dispatcher )
        {
            m_dispatcher->UnregisterFD(m_wakeupPipe->GetReadFd());
        }

        delete m_wakeupPipe;
    }
}

//-----------------------------------------------------------------------------
// adding & removing sources
//-----------------------------------------------------------------------------

#if wxUSE_EVENTLOOP_SOURCE

// This class is a temporary bridge between event loop sources and
// FDIODispatcher. It is going to be removed soon, when all subject interfaces
// are modified
class wxFDIOEventLoopSourceHandler : public wxFDIOHandler
{
public:
    wxFDIOEventLoopSourceHandler(wxEventLoopSourceHandler* handler) :
        m_impl(handler) { }

    virtual void OnReadWaiting()
    {
        m_impl->OnReadWaiting();
    }
    virtual void OnWriteWaiting()
    {
        m_impl->OnWriteWaiting();
    }

    virtual void OnExceptionWaiting()
    {
        m_impl->OnExceptionWaiting();
    }

protected:
    wxEventLoopSourceHandler* m_impl;
};

wxEventLoopSource *
wxConsoleEventLoop::AddSourceForFD(int fd,
                                   wxEventLoopSourceHandler *handler,
                                   int flags)
{
    wxCHECK_MSG( fd != -1, NULL, "can't monitor invalid fd" );

    wxLogTrace(wxTRACE_EVT_SOURCE,
                "Adding event loop source for fd=%d", fd);

    // we need a bridge to wxFDIODispatcher
    //
    // TODO: refactor the code so that only wxEventLoopSourceHandler is used
    wxScopedPtr<wxFDIOHandler>
        fdioHandler(new wxFDIOEventLoopSourceHandler(handler));

    if ( !m_dispatcher->RegisterFD(fd, fdioHandler.get(), flags) )
        return NULL;

    return new wxUnixEventLoopSource(m_dispatcher, fdioHandler.release(),
                                     fd, handler, flags);
}

wxUnixEventLoopSource::~wxUnixEventLoopSource()
{
    wxLogTrace(wxTRACE_EVT_SOURCE,
               "Removing event loop source for fd=%d", m_fd);

    m_dispatcher->UnregisterFD(m_fd);

    delete m_fdioHandler;
}

#endif // wxUSE_EVENTLOOP_SOURCE

//-----------------------------------------------------------------------------
// events dispatch and loop handling
//-----------------------------------------------------------------------------

bool wxConsoleEventLoop::Pending() const
{
    if ( m_dispatcher->HasPending() )
        return true;

#if wxUSE_TIMER
    wxUsecClock_t nextTimer;
    if ( wxTimerScheduler::Get().GetNext(&nextTimer) &&
            !wxMilliClockToLong(nextTimer) )
        return true;
#endif // wxUSE_TIMER

    return false;
}

bool wxConsoleEventLoop::Dispatch()
{
    DispatchTimeout(static_cast<unsigned long>(
        wxFDIODispatcher::TIMEOUT_INFINITE));

    return true;
}

int wxConsoleEventLoop::DispatchTimeout(unsigned long timeout)
{
#if wxUSE_TIMER
    // check if we need to decrease the timeout to account for a timer
    wxUsecClock_t nextTimer;
    if ( wxTimerScheduler::Get().GetNext(&nextTimer) )
    {
        unsigned long timeUntilNextTimer = wxMilliClockToLong(nextTimer / 1000);
        if ( timeUntilNextTimer < timeout )
            timeout = timeUntilNextTimer;
    }
#endif // wxUSE_TIMER

    bool hadEvent = m_dispatcher->Dispatch(timeout) > 0;

#if wxUSE_TIMER
    if ( wxTimerScheduler::Get().NotifyExpired() )
        hadEvent = true;
#endif // wxUSE_TIMER

    return hadEvent ? 1 : -1;
}

void wxConsoleEventLoop::WakeUp()
{
    m_wakeupPipe->WakeUp();
}

void wxConsoleEventLoop::OnNextIteration()
{
    // call the signal handlers for any signals we caught recently
    wxTheApp->CheckSignal();
}


wxEventLoopBase *wxConsoleAppTraits::CreateEventLoop()
{
    return new wxEventLoop();
}

#endif // wxUSE_CONSOLE_EVENTLOOP
