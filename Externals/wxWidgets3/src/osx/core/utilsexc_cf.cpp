/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/utilsexc_cf.cpp
// Purpose:     Execution-related utilities for Darwin
// Author:      David Elliott, Ryan Norton (wxMacExecute)
// Modified by: Stefan Csomor (added necessary wxT for unicode builds)
// Created:     2004-11-04
// Copyright:   (c) David Elliott, Ryan Norton
//              (c) 2013 Rob Bresalier
// Licence:     wxWindows licence
// Notes:       This code comes from src/osx/carbon/utilsexc.cpp,1.11
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/utils.h"
#endif //ndef WX_PRECOMP
#include "wx/stdpaths.h"
#include "wx/app.h"
#include "wx/apptrait.h"
#include "wx/thread.h"
#include "wx/process.h"

#include "wx/evtloop.h"
#include "wx/evtloopsrc.h"
#include "wx/private/eventloopsourcesmanager.h"

#include <sys/wait.h>

#include <CoreFoundation/CFSocket.h>

#if wxUSE_EVENTLOOP_SOURCE

namespace
{

extern "C"
void
wx_socket_callback(CFSocketRef WXUNUSED(s),
                   CFSocketCallBackType callbackType,
                   CFDataRef WXUNUSED(address),
                   void const *WXUNUSED(data),
                   void *ctxData)
{
    wxLogTrace(wxTRACE_EVT_SOURCE,
               "CFSocket callback, type=%d", static_cast<int>(callbackType));

    wxCFEventLoopSource * const
        source = static_cast<wxCFEventLoopSource *>(ctxData);

    wxEventLoopSourceHandler * const
        handler = source->GetHandler();

    switch ( callbackType )
    {
        case kCFSocketReadCallBack:
            handler->OnReadWaiting();
            break;

        case kCFSocketWriteCallBack:
            handler->OnWriteWaiting();
            break;

        default:
            wxFAIL_MSG( "Unexpected callback type." );
    }
}

} // anonymous namespace

class wxCFEventLoopSourcesManager : public wxEventLoopSourcesManagerBase
{
public:
    wxEventLoopSource *
    AddSourceForFD(int fd, wxEventLoopSourceHandler *handler, int flags) wxOVERRIDE
    {
        wxCHECK_MSG( fd != -1, NULL, "can't monitor invalid fd" );

        wxScopedPtr<wxCFEventLoopSource>
            source(new wxCFEventLoopSource(handler, flags));

        CFSocketContext context = { 0, source.get(), NULL, NULL, NULL };

        int callbackTypes = 0;
        if ( flags & wxEVENT_SOURCE_INPUT )
            callbackTypes |= kCFSocketReadCallBack;
        if ( flags & wxEVENT_SOURCE_OUTPUT )
            callbackTypes |= kCFSocketWriteCallBack;

        wxCFRef<CFSocketRef>
            cfSocket(CFSocketCreateWithNative
                     (
                        kCFAllocatorDefault,
                        fd,
                        callbackTypes,
                        &wx_socket_callback,
                        &context
                      ));

        if ( !cfSocket )
        {
            wxLogError(wxS("Failed to create event loop source socket."));
            return NULL;
        }

        // Adjust the socket options to suit our needs:
        CFOptionFlags sockopt = CFSocketGetSocketFlags(cfSocket);

        // First, by default, write callback is not called repeatedly when data
        // can be written to the socket but we need this behaviour so request
        // it explicitly.
        if ( flags & wxEVENT_SOURCE_OUTPUT )
            sockopt |= kCFSocketAutomaticallyReenableWriteCallBack;

        // Second, we use the socket to monitor the FD but it doesn't own it,
        // so prevent the FD from being closed when the socket is invalidated.
        sockopt &= ~kCFSocketCloseOnInvalidate;

        CFSocketSetSocketFlags(cfSocket, sockopt);

        wxCFRef<CFRunLoopSourceRef>
            runLoopSource(CFSocketCreateRunLoopSource
                          (
                            kCFAllocatorDefault,
                            cfSocket,
                            0 // Lowest index means highest priority
                          ));
        if ( !runLoopSource )
        {
            wxLogError(wxS("Failed to create low level event loop source."));
            CFSocketInvalidate(cfSocket);
            return NULL;
        }

        // Save the socket so that we can remove it later if asked to.
        source->InitSourceSocket(cfSocket.release());

        CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);

        return source.release();
    }
};

wxEventLoopSourcesManagerBase* wxGUIAppTraits::GetEventLoopSourcesManager()
{
    static wxCFEventLoopSourcesManager s_eventLoopSourcesManager;

    return &s_eventLoopSourcesManager;
}

#endif // wxUSE_EVENTLOOP_SOURCE

/////////////////////////////////////////////////////////////////////////////

#if wxUSE_SOCKETS

// we need to implement this method in a file of the core library as it should
// only be used for the GUI applications but we can't use socket stuff from it
// directly as this would create unwanted dependencies of core on net library
//
// so we have this global pointer which is set from sockosx.cpp when it is
// linked in and we simply return it from here
extern WXDLLIMPEXP_BASE wxSocketManager *wxOSXSocketManagerCF;
wxSocketManager *wxGUIAppTraits::GetSocketManager()
{
    return wxOSXSocketManagerCF ? wxOSXSocketManagerCF
                                : wxGUIAppTraitsBase::GetSocketManager();
}

#endif // wxUSE_SOCKETS
