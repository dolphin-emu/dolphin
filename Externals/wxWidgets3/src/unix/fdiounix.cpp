///////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/fdiounix.cpp
// Purpose:     wxFDIOManager implementation for console Unix applications
// Author:      Vadim Zeitlin
// Created:     2009-08-17
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_SOCKETS

#include "wx/apptrait.h"
#include "wx/log.h"
#include "wx/private/fdiodispatcher.h"
#include "wx/unix/private/fdiounix.h"

// ============================================================================
// wxFDIOManagerUnix implementation
// ============================================================================

int wxFDIOManagerUnix::AddInput(wxFDIOHandler *handler, int fd, Direction d)
{
    wxFDIODispatcher * const dispatcher = wxFDIODispatcher::Get();
    wxCHECK_MSG( dispatcher, -1, "can't monitor FDs without FD IO dispatcher" );

    // translate our direction to dispatcher flags
    const int flag = d == INPUT ? wxFDIO_INPUT : wxFDIO_OUTPUT;

    // we need to either register this FD with the dispatcher or update an
    // existing registration depending on whether it had been previously
    // registered for anything or not
    bool ok;
    const int regmask = handler->GetRegisteredEvents();
    if ( !regmask )
    {
        ok = dispatcher->RegisterFD(fd, handler, flag);
    }
    else
    {
        ok = dispatcher->ModifyFD(fd, handler, regmask | flag);
    }

    if ( !ok )
        return -1;

    // update the stored mask of registered events
    handler->SetRegisteredEvent(flag);

    return fd;
}

void wxFDIOManagerUnix::RemoveInput(wxFDIOHandler *handler, int fd, Direction d)
{
    wxFDIODispatcher * const dispatcher = wxFDIODispatcher::Get();
    if ( !dispatcher )
        return;

    const int flag = d == INPUT ? wxFDIO_INPUT : wxFDIO_OUTPUT;

    // just as in AddInput() above we may need to either just modify the FD or
    // remove it completely if we don't need to monitor it any more
    bool ok;
    const int regmask = handler->GetRegisteredEvents();
    if ( regmask == flag )
    {
        ok = dispatcher->UnregisterFD(fd);
    }
    else
    {
        ok = dispatcher->ModifyFD(fd, handler, regmask & ~flag);
    }

    if ( !ok )
    {
        wxLogDebug("Failed to unregister %d in direction %d", fd, d);
    }

    // do this even after a failure to unregister it, we still tried...
    handler->ClearRegisteredEvent(flag);
}

wxFDIOManager *wxAppTraits::GetFDIOManager()
{
    static wxFDIOManagerUnix s_manager;
    return &s_manager;
}

#endif // wxUSE_SOCKETS
