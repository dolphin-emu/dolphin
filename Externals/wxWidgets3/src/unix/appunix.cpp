/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/appunix.cpp
// Purpose:     wxAppConsole with wxMainLoop implementation
// Author:      Lukasz Michalski
// Created:     28/01/2005
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
#endif

#include "wx/evtloop.h"
#include "wx/scopedptr.h"
#include "wx/unix/private/wakeuppipe.h"
#include "wx/private/fdiodispatcher.h"
#include "wx/private/fdioeventloopsourcehandler.h"

#include <signal.h>
#include <unistd.h>

#ifndef SA_RESTART
    // don't use for systems which don't define it (at least VMS and QNX)
    #define SA_RESTART 0
#endif

// ----------------------------------------------------------------------------
// Helper class calling CheckSignal() on wake up
// ----------------------------------------------------------------------------

namespace
{

class SignalsWakeUpPipe : public wxWakeUpPipe
{
public:
    // Ctor automatically registers this pipe with the event loop.
    SignalsWakeUpPipe()
    {
        m_source = wxEventLoopBase::AddSourceForFD
                                    (
                                        GetReadFd(),
                                        this,
                                        wxEVENT_SOURCE_INPUT
                                    );
    }

    virtual void OnReadWaiting() wxOVERRIDE
    {
        // The base class wxWakeUpPipe::OnReadWaiting() needs to be called in order
        // to read the data out of the wake up pipe and clear it for next time.
        wxWakeUpPipe::OnReadWaiting();

        if ( wxTheApp )
            wxTheApp->CheckSignal();
    }

    virtual ~SignalsWakeUpPipe()
    {
        delete m_source;
    }

private:
    wxEventLoopSource* m_source;
};

} // anonymous namespace

wxAppConsole::wxAppConsole()
{
    m_signalWakeUpPipe = NULL;
}

wxAppConsole::~wxAppConsole()
{
    delete m_signalWakeUpPipe;
}

// use unusual names for arg[cv] to avoid clashes with wxApp members with the
// same names
bool wxAppConsole::Initialize(int& argc_, wxChar** argv_)
{
    if ( !wxAppConsoleBase::Initialize(argc_, argv_) )
        return false;

    sigemptyset(&m_signalsCaught);

    return true;
}

// The actual signal handler. It does as little as possible (because very few
// things are safe to do from inside a signal handler) and just ensures that
// CheckSignal() will be called later from SignalsWakeUpPipe::OnReadWaiting().
void wxAppConsole::HandleSignal(int signal)
{
    wxAppConsole * const app = wxTheApp;
    if ( !app )
        return;

    // Register the signal that is caught.
    sigaddset(&(app->m_signalsCaught), signal);

    // Wake up the application for handling the signal.
    //
    // Notice that we must have a valid wake up pipe here as we only install
    // our signal handlers after allocating it.
    app->m_signalWakeUpPipe->WakeUpNoLock();
}

void wxAppConsole::CheckSignal()
{
    for ( SignalHandlerHash::iterator it = m_signalHandlerHash.begin();
          it != m_signalHandlerHash.end();
          ++it )
    {
        int sig = it->first;
        if ( sigismember(&m_signalsCaught, sig) )
        {
            sigdelset(&m_signalsCaught, sig);
            (it->second)(sig);
        }
    }
}

wxFDIOHandler* wxAppConsole::RegisterSignalWakeUpPipe(wxFDIODispatcher& dispatcher)
{
    wxCHECK_MSG( m_signalWakeUpPipe, NULL, "Should be allocated" );

    // we need a bridge to wxFDIODispatcher
    //
    // TODO: refactor the code so that only wxEventLoopSourceHandler is used
    wxScopedPtr<wxFDIOHandler>
        fdioHandler(new wxFDIOEventLoopSourceHandler(m_signalWakeUpPipe));

    if ( !dispatcher.RegisterFD
                     (
                      m_signalWakeUpPipe->GetReadFd(),
                      fdioHandler.get(),
                      wxFDIO_INPUT
                     ) )
        return NULL;

    return fdioHandler.release();
}

// the type of the signal handlers we use is "void(*)(int)" while the real
// signal handlers are extern "C" and so have incompatible type and at least
// Sun CC warns about it, so use explicit casts to suppress these warnings as
// they should be harmless
extern "C"
{
    typedef void (*SignalHandler_t)(int);
}

bool wxAppConsole::SetSignalHandler(int signal, SignalHandler handler)
{
    const bool install = (SignalHandler_t)handler != SIG_DFL &&
                         (SignalHandler_t)handler != SIG_IGN;

    if ( !m_signalWakeUpPipe )
    {
        // Create the pipe that the signal handler will use to cause the event
        // loop to call wxAppConsole::CheckSignal().
        m_signalWakeUpPipe = new SignalsWakeUpPipe();
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (SignalHandler_t)&wxAppConsole::HandleSignal;
    sa.sa_flags = SA_RESTART;
    int res = sigaction(signal, &sa, 0);
    if ( res != 0 )
    {
        wxLogSysError(_("Failed to install signal handler"));
        return false;
    }

    if ( install )
        m_signalHandlerHash[signal] = handler;
    else
        m_signalHandlerHash.erase(signal);

    return true;
}

