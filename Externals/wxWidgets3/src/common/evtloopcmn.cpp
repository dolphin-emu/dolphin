///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/evtloopcmn.cpp
// Purpose:     common wxEventLoop-related stuff
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2006-01-12
// RCS-ID:      $Id: evtloopcmn.cpp 66229 2010-11-22 01:22:56Z VZ $
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/evtloop.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif //WX_PRECOMP

// ----------------------------------------------------------------------------
// wxEventLoopBase
// ----------------------------------------------------------------------------

wxEventLoopBase *wxEventLoopBase::ms_activeLoop = NULL;

wxEventLoopBase::wxEventLoopBase()
{
    m_isInsideYield = false;
    m_eventsToProcessInsideYield = wxEVT_CATEGORY_ALL;
}

bool wxEventLoopBase::IsMain() const
{
    if (wxTheApp)
        return wxTheApp->GetMainLoop() == this;
    return false;
}

/* static */
void wxEventLoopBase::SetActive(wxEventLoopBase* loop)
{
    ms_activeLoop = loop;

    if (wxTheApp)
        wxTheApp->OnEventLoopEnter(loop);
}

void wxEventLoopBase::OnExit()
{
    if (wxTheApp)
        wxTheApp->OnEventLoopExit(this);
}

void wxEventLoopBase::WakeUpIdle()
{
    WakeUp();
}

bool wxEventLoopBase::ProcessIdle()
{
    return wxTheApp && wxTheApp->ProcessIdle();
}

bool wxEventLoopBase::Yield(bool onlyIfNeeded)
{
    if ( m_isInsideYield )
    {
        if ( !onlyIfNeeded )
        {
            wxFAIL_MSG( wxT("wxYield called recursively" ) );
        }

        return false;
    }

    return YieldFor(wxEVT_CATEGORY_ALL);
}

// wxEventLoopManual is unused in the other ports
#if defined(__WXMSW__) || defined(__WXDFB__) || ( ( defined(__UNIX__) && !defined(__WXOSX__) ) && wxUSE_BASE)

// ============================================================================
// wxEventLoopManual implementation
// ============================================================================

wxEventLoopManual::wxEventLoopManual()
{
    m_exitcode = 0;
    m_shouldExit = false;
}

bool wxEventLoopManual::ProcessEvents()
{
    // process pending wx events first as they correspond to low-level events
    // which happened before, i.e. typically pending events were queued by a
    // previous call to Dispatch() and if we didn't process them now the next
    // call to it might enqueue them again (as happens with e.g. socket events
    // which would be generated as long as there is input available on socket
    // and this input is only removed from it when pending event handlers are
    // executed)
    if ( wxTheApp )
        wxTheApp->ProcessPendingEvents();

    return Dispatch();
}

int wxEventLoopManual::Run()
{
    // event loops are not recursive, you need to create another loop!
    wxCHECK_MSG( !IsRunning(), -1, wxT("can't reenter a message loop") );

    // ProcessIdle() and ProcessEvents() below may throw so the code here should
    // be exception-safe, hence we must use local objects for all actions we
    // should undo
    wxEventLoopActivator activate(this);

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

            // this is the event loop itself
            for ( ;; )
            {
                // give them the possibility to do whatever they want
                OnNextIteration();

                // generate and process idle events for as long as we don't
                // have anything else to do
                while ( !Pending() && ProcessIdle() && !m_shouldExit )
                    ;

                // if the "should exit" flag is set, the loop should terminate
                // but not before processing any remaining messages so while
                // Pending() returns true, do process them
                if ( m_shouldExit )
                {
                    while ( Pending() )
                        ProcessEvents();

                    break;
                }

                // a message came or no more idle processing to do, dispatch
                // all the pending events and call Dispatch() to wait for the
                // next message
                if ( !ProcessEvents() )
                {
                    // we got WM_QUIT
                    break;
                }
            }

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

void wxEventLoopManual::Exit(int rc)
{
    wxCHECK_RET( IsRunning(), wxT("can't call Exit() if not running") );

    m_exitcode = rc;
    m_shouldExit = true;

    OnExit();

    // all we have to do to exit from the loop is to (maybe) wake it up so that
    // it can notice that Exit() had been called
    //
    // in particular, do *not* use here calls such as PostQuitMessage() (under
    // MSW) which terminate the current event loop here because we're not sure
    // that it is going to be processed by the correct event loop: it would be
    // possible that another one is started and terminated by mistake if we do
    // this
    WakeUp();
}

#endif // __WXMSW__ || __WXMAC__ || __WXDFB__

