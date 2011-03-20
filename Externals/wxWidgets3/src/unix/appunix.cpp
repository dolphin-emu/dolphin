/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/appunix.cpp
// Purpose:     wxAppConsole with wxMainLoop implementation
// Author:      Lukasz Michalski
// Created:     28/01/2005
// RCS-ID:      $Id: appunix.cpp 67254 2011-03-20 00:14:35Z DS $
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

#include <signal.h>
#include <unistd.h>

#ifndef SA_RESTART
    // don't use for systems which don't define it (at least VMS and QNX)
    #define SA_RESTART 0
#endif

// use unusual names for arg[cv] to avoid clashes with wxApp members with the
// same names
bool wxAppConsole::Initialize(int& argc_, wxChar** argv_)
{
    if ( !wxAppConsoleBase::Initialize(argc_, argv_) )
        return false;

    sigemptyset(&m_signalsCaught);

    return true;
}

void wxAppConsole::HandleSignal(int signal)
{
    wxAppConsole * const app = wxTheApp;
    if ( !app )
        return;

    sigaddset(&(app->m_signalsCaught), signal);
    app->WakeUpIdle();
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

