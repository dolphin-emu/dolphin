/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/sockunix.cpp
// Purpose:     wxSocketImpl implementation for Unix systems
// Authors:     Guilhem Lavaux, Guillermo Rodriguez Garcia, David Elliott,
//              Vadim Zeitlin
// Created:     April 1997
// Copyright:   (c) 1997 Guilhem Lavaux
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#include "wx/wxprec.h"

#if wxUSE_SOCKETS

#include "wx/private/fd.h"
#include "wx/private/socket.h"
#include "wx/unix/private/sockunix.h"

#include <errno.h>

#include <sys/types.h>

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

#ifndef WX_SOCKLEN_T

#ifdef VMS
#  define WX_SOCKLEN_T unsigned int
#else
#  ifdef __GLIBC__
#    if __GLIBC__ == 2
#      define WX_SOCKLEN_T socklen_t
#    endif
#  elif defined(__WXMAC__)
#    define WX_SOCKLEN_T socklen_t
#  else
#    define WX_SOCKLEN_T int
#  endif
#endif

#endif /* SOCKLEN_T */

#ifndef SOCKOPTLEN_T
    #define SOCKOPTLEN_T WX_SOCKLEN_T
#endif

// UnixWare reportedly needs this for FIONBIO definition
#ifdef __UNIXWARE__
    #include <sys/filio.h>
#endif

// ============================================================================
// wxSocketImpl implementation
// ============================================================================

wxSocketError wxSocketImplUnix::GetLastError() const
{
    switch ( errno )
    {
        case 0:
            return wxSOCKET_NOERROR;

        case ENOTSOCK:
            return wxSOCKET_INVSOCK;

        // unfortunately EAGAIN only has the "would block" meaning for read(),
        // not for connect() for which it means something rather different but
        // we can't distinguish between these two situations currently...
        //
        // also notice that EWOULDBLOCK can be different from EAGAIN on some
        // systems (HP-UX being the only known example) while it's defined as
        // EAGAIN on most others (e.g. Linux)
        case EAGAIN:
#ifdef EWOULDBLOCK
    #if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
    #endif
#endif // EWOULDBLOCK
        case EINPROGRESS:
            return wxSOCKET_WOULDBLOCK;

        default:
            return wxSOCKET_IOERR;
    }
}

void wxSocketImplUnix::DoEnableEvents(int flags, bool enable)
{
    // No events for blocking sockets, they should be usable from the other
    // threads and the events only work for the sockets used by the main one.
    if ( GetSocketFlags() & wxSOCKET_BLOCK )
        return;

    wxSocketManager * const manager = wxSocketManager::Get();
    if (!manager)
        return;

    if ( enable )
    {
        if ( flags & wxSOCKET_INPUT_FLAG )
            manager->Install_Callback(this, wxSOCKET_INPUT);
        if ( flags & wxSOCKET_OUTPUT_FLAG )
            manager->Install_Callback(this, wxSOCKET_OUTPUT);
    }
    else // off
    {
        if ( flags & wxSOCKET_INPUT_FLAG )
            manager->Uninstall_Callback(this, wxSOCKET_INPUT);
        if ( flags & wxSOCKET_OUTPUT_FLAG )
            manager->Uninstall_Callback(this, wxSOCKET_OUTPUT);
    }
}

int wxSocketImplUnix::CheckForInput()
{
    char c;
    int rc;
    do
    {
        rc = recv(m_fd, &c, 1, MSG_PEEK);
    } while ( rc == -1 && errno == EINTR );

    return rc;
}

void wxSocketImplUnix::OnStateChange(wxSocketNotify event)
{
    NotifyOnStateChange(event);

    if ( event == wxSOCKET_LOST )
        Shutdown();
}

void wxSocketImplUnix::OnReadWaiting()
{
    wxASSERT_MSG( m_fd != INVALID_SOCKET, "invalid socket ready for reading?" );

    // we need to disable the read notifications until we read all the data
    // already available for the socket, otherwise we're going to keep getting
    // them continuously which is worse than inefficient: as IO notifications
    // have higher priority than idle events in e.g. GTK+, our pending events
    // whose handlers typically call Read() which would consume the data and so
    // stop the notifications flood would never be dispatched at all if the
    // notifications were not disabled
    DisableEvents(wxSOCKET_INPUT_FLAG);


    // find out what are we going to notify about exactly
    wxSocketNotify notify;

    // TCP listening sockets become ready for reading when there is a pending
    // connection
    if ( m_server && m_stream )
    {
        notify = wxSOCKET_CONNECTION;
    }
    else // check if there is really any input available
    {
        switch ( CheckForInput() )
        {
            case 1:
                notify = wxSOCKET_INPUT;
                break;

            case 0:
                // reading 0 bytes for a TCP socket means that the connection
                // was closed by peer but for UDP it just means that we got an
                // empty datagram
                notify = m_stream ? wxSOCKET_LOST : wxSOCKET_INPUT;
                break;

            default:
                wxFAIL_MSG( "unexpected CheckForInput() return value" );
                wxFALLTHROUGH;

            case -1:
                if ( GetLastError() == wxSOCKET_WOULDBLOCK )
                {
                    // just a spurious wake up
                    EnableEvents(wxSOCKET_INPUT_FLAG);
                    return;
                }

                notify = wxSOCKET_LOST;
        }
    }

    OnStateChange(notify);
}

void wxSocketImplUnix::OnWriteWaiting()
{
    wxASSERT_MSG( m_fd != INVALID_SOCKET, "invalid socket ready for writing?" );

    // see comment in the beginning of OnReadWaiting() above
    DisableEvents(wxSOCKET_OUTPUT_FLAG);


    // check whether this is a notification for the completion of a
    // non-blocking connect()
    if ( m_establishing && !m_server )
    {
        m_establishing = false;

        // check whether we connected successfully
        int error;
        SOCKOPTLEN_T len = sizeof(error);

        getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

        if ( error )
        {
            OnStateChange(wxSOCKET_LOST);
            return;
        }

        OnStateChange(wxSOCKET_CONNECTION);
    }

    OnStateChange(wxSOCKET_OUTPUT);
}

void wxSocketImplUnix::OnExceptionWaiting()
{
    // when using epoll() this is called when an error occurred on the socket
    // so close it if it hadn't been done yet -- what else can we do?
    //
    // notice that we shouldn't be called at all when using select() as we
    // don't use wxFDIO_EXCEPTION when registering the socket for monitoring
    // and this is good because select() would call this for any OOB data which
    // is not necessarily an error
    if ( m_fd != INVALID_SOCKET )
        OnStateChange(wxSOCKET_LOST);
}

#endif  /* wxUSE_SOCKETS */
