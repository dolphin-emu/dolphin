/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/app.h
// Purpose:     wxAppConsole implementation for Unix
// Author:      Lukasz Michalski
// Created:     28/01/2005
// RCS-ID:      $Id: app.h 56994 2008-11-28 12:47:07Z VZ $
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//Ensure that sigset_t is being defined
#include <signal.h>

// wxApp subclass implementing event processing for console applications
class WXDLLIMPEXP_BASE wxAppConsole : public wxAppConsoleBase
{
public:
    // override base class initialization
    virtual bool Initialize(int& argc, wxChar** argv);


    // Unix-specific: Unix signal handling
    // -----------------------------------

    // type of the function which can be registered as signal handler: notice
    // that it isn't really a signal handler, i.e. it's not subject to the
    // usual signal handlers constraints, because it is called later from
    // CheckSignal() and not when the signal really occurs
    typedef void (*SignalHandler)(int);

    // Set signal handler for the given signal, SIG_DFL or SIG_IGN can be used
    // instead of a function pointer
    //
    // Return true if handler was installed, false on error
    bool SetSignalHandler(int signal, SignalHandler handler);

    // Check if any Unix signals arrived since the last call and execute
    // handlers for them
    void CheckSignal();

private:
    // signal handler set up by SetSignalHandler() for all signals we handle,
    // it just adds the signal to m_signalsCaught -- the real processing is
    // done later, when CheckSignal() is called
    static void HandleSignal(int signal);


    // signals for which HandleSignal() had been called (reset from
    // CheckSignal())
    sigset_t m_signalsCaught;

    // the signal handlers
    WX_DECLARE_HASH_MAP(int, SignalHandler, wxIntegerHash, wxIntegerEqual, SignalHandlerHash);
    SignalHandlerHash m_signalHandlerHash;
};
