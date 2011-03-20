///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/evtloop.cpp
// Purpose:     implements wxEventLoop for GTK+
// Author:      Vadim Zeitlin
// Modified by:
// Created:     10.07.01
// RCS-ID:      $Id: evtloop.cpp 66090 2010-11-10 13:52:17Z VZ $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/evtloop.h"
#include "wx/evtloopsrc.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include <gtk/gtk.h>
#include <glib.h>

// ============================================================================
// wxEventLoop implementation
// ============================================================================

extern GtkWidget *wxGetRootWindow();

// ----------------------------------------------------------------------------
// wxEventLoop running and exiting
// ----------------------------------------------------------------------------

wxGUIEventLoop::wxGUIEventLoop()
{
    m_exitcode = 0;
}

int wxGUIEventLoop::Run()
{
    // event loops are not recursive, you need to create another loop!
    wxCHECK_MSG( !IsRunning(), -1, "can't reenter a message loop" );

    wxEventLoopActivator activate(this);

    gtk_main();

    OnExit();

    return m_exitcode;
}

void wxGUIEventLoop::Exit(int rc)
{
    wxCHECK_RET( IsRunning(), "can't call Exit() if not running" );

    m_exitcode = rc;

    gtk_main_quit();
}

void wxGUIEventLoop::WakeUp()
{
    // TODO: idle events handling should really be done by wxEventLoop itself
    //       but for now it's completely in gtk/app.cpp so just call there when
    //       we have wxTheApp and hope that it doesn't matter that we do
    //       nothing when we don't...
    if ( wxTheApp )
        wxTheApp->WakeUpIdle();
}

// ----------------------------------------------------------------------------
// wxEventLoop adding & removing sources
// ----------------------------------------------------------------------------

#if wxUSE_EVENTLOOP_SOURCE

extern "C"
{
static gboolean wx_on_channel_event(GIOChannel *channel,
                                    GIOCondition condition,
                                    gpointer data)
{
    wxUnusedVar(channel); // Unused if !wxUSE_LOG || !wxDEBUG_LEVEL

    wxLogTrace(wxTRACE_EVT_SOURCE,
               "wx_on_channel_event, fd=%d, condition=%08x",
               g_io_channel_unix_get_fd(channel), condition);

    wxEventLoopSourceHandler * const
        handler = static_cast<wxEventLoopSourceHandler *>(data);

    if (condition & G_IO_IN || condition & G_IO_PRI)
        handler->OnReadWaiting();
    if (condition & G_IO_OUT)
        handler->OnWriteWaiting();
    else if (condition & G_IO_ERR || condition & G_IO_NVAL)
        handler->OnExceptionWaiting();

    // we never want to remove source here, so always return true
    return TRUE;
}
}

wxEventLoopSource *
wxGUIEventLoop::AddSourceForFD(int fd,
                               wxEventLoopSourceHandler *handler,
                               int flags)
{
    wxCHECK_MSG( fd != -1, NULL, "can't monitor invalid fd" );

    int condition = 0;
    if (flags & wxEVENT_SOURCE_INPUT)
        condition |= G_IO_IN | G_IO_PRI;
    if (flags & wxEVENT_SOURCE_OUTPUT)
        condition |= G_IO_OUT;
    if (flags & wxEVENT_SOURCE_EXCEPTION)
        condition |= G_IO_ERR | G_IO_HUP | G_IO_NVAL;

    GIOChannel* channel = g_io_channel_unix_new(fd);
    const unsigned sourceId  = g_io_add_watch
                               (
                                channel,
                                (GIOCondition)condition,
                                &wx_on_channel_event,
                                handler
                               );
    // it was ref'd by g_io_add_watch() so we can unref it here
    g_io_channel_unref(channel);

    if ( !sourceId )
        return NULL;

    wxLogTrace(wxTRACE_EVT_SOURCE,
               "Adding event loop source for fd=%d with GTK id=%u",
               fd, sourceId);


    return new wxGTKEventLoopSource(sourceId, handler, flags);
}

wxGTKEventLoopSource::~wxGTKEventLoopSource()
{
    wxLogTrace(wxTRACE_EVT_SOURCE,
               "Removing event loop source with GTK id=%u", m_sourceId);

    g_source_remove(m_sourceId);
}

#endif // wxUSE_EVENTLOOP_SOURCE

// ----------------------------------------------------------------------------
// wxEventLoop message processing dispatching
// ----------------------------------------------------------------------------

bool wxGUIEventLoop::Pending() const
{
    if ( wxTheApp )
    {
        // this avoids false positives from our idle source
        return wxTheApp->EventsPending();
    }

    return gtk_events_pending() != 0;
}

bool wxGUIEventLoop::Dispatch()
{
    wxCHECK_MSG( IsRunning(), false, wxT("can't call Dispatch() if not running") );

    // gtk_main_iteration() returns TRUE only if gtk_main_quit() was called
    return !gtk_main_iteration();
}

extern "C" {
static gboolean wx_event_loop_timeout(void* data)
{
    bool* expired = static_cast<bool*>(data);
    *expired = true;

    // return FALSE to remove this timeout
    return FALSE;
}
}

int wxGUIEventLoop::DispatchTimeout(unsigned long timeout)
{
    bool expired = false;
    const unsigned id = g_timeout_add(timeout, wx_event_loop_timeout, &expired);
    bool quit = gtk_main_iteration() != 0;

    if ( expired )
        return -1;

    g_source_remove(id);

    return !quit;
}

//-----------------------------------------------------------------------------
// YieldFor
//-----------------------------------------------------------------------------

extern "C" {
static void wxgtk_main_do_event(GdkEvent* event, void* data)
{
    // categorize the GDK event according to wxEventCategory.
    // See http://library.gnome.org/devel/gdk/unstable/gdk-Events.html#GdkEventType
    // for more info.

    // NOTE: GDK_* constants which were not present in the GDK2.0 can be tested for
    //       only at compile-time; when running the program (compiled with a recent GDK)
    //       on a system with an older GDK lib we can be sure there won't be problems
    //       because event->type will never assume those values corresponding to
    //       new event types (since new event types are always added in GDK with non
    //       conflicting values for ABI compatibility).

    wxEventCategory cat = wxEVT_CATEGORY_UNKNOWN;
    switch (event->type)
    {
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
    case GDK_SELECTION_CLEAR:
#if GTK_CHECK_VERSION(2,6,0)
    case GDK_OWNER_CHANGE:
#endif
        cat = wxEVT_CATEGORY_CLIPBOARD;
        break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_SCROLL:        // generated from mouse buttons
    case GDK_CLIENT_EVENT:
        cat = wxEVT_CATEGORY_USER_INPUT;
        break;

    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:

    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_VISIBILITY_NOTIFY:
    case GDK_PROPERTY_NOTIFY:

    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_WINDOW_STATE:
    case GDK_SETTING:
    case GDK_DELETE:
    case GDK_DESTROY:

    case GDK_EXPOSE:
    case GDK_NO_EXPOSE:
    case GDK_MAP:
    case GDK_UNMAP:

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
#if GTK_CHECK_VERSION(2,8,0)
    case GDK_GRAB_BROKEN:
#endif
#if GTK_CHECK_VERSION(2,14,0)
    case GDK_DAMAGE:
#endif
        cat = wxEVT_CATEGORY_UI;
        break;

    default:
        cat = wxEVT_CATEGORY_UNKNOWN;
        break;
    }

    wxGUIEventLoop* evtloop = static_cast<wxGUIEventLoop*>(data);

    // is this event allowed now?
    if (evtloop->IsEventAllowedInsideYield(cat))
        gtk_main_do_event(event);         // process it now
    else if (event->type != GDK_NOTHING)
        evtloop->StoreGdkEventForLaterProcessing(gdk_event_copy(event));
            // process it later (but make a copy; the caller will free the event pointer)
}
}

bool wxGUIEventLoop::YieldFor(long eventsToProcess)
{
#if wxUSE_THREADS
    if ( !wxThread::IsMain() )
    {
        // can't call gtk_main_iteration() from other threads like this
        return true;
    }
#endif // wxUSE_THREADS

    m_isInsideYield = true;
    m_eventsToProcessInsideYield = eventsToProcess;

#if wxUSE_LOG
    // disable log flushing from here because a call to wxYield() shouldn't
    // normally result in message boxes popping up &c
    wxLog::Suspend();
#endif

    // temporarily replace the global GDK event handler with our function, which
    // categorizes the events and using m_eventsToProcessInsideYield decides
    // if an event should be processed immediately or not
    // NOTE: this approach is better than using gdk_display_get_event() because
    //       gtk_main_iteration() does more than just calling gdk_display_get_event()
    //       and then call gtk_main_do_event()!
    //       In particular in this way we also process input from sources like
    //       GIOChannels (this is needed for e.g. wxGUIAppTraits::WaitForChild).
    gdk_event_handler_set(wxgtk_main_do_event, this, NULL);
    while (Pending())   // avoid false positives from our idle source
        gtk_main_iteration();
    gdk_event_handler_set ((GdkEventFunc)gtk_main_do_event, NULL, NULL);

    if (eventsToProcess != wxEVT_CATEGORY_CLIPBOARD)
    {
        // It's necessary to call ProcessIdle() to update the frames sizes which
        // might have been changed (it also will update other things set from
        // OnUpdateUI() which is a nice (and desired) side effect). But we
        // call ProcessIdle() only once since this is not meant for longish
        // background jobs (controlled by wxIdleEvent::RequestMore() and the
        // return value of Processidle().
        ProcessIdle();      // ProcessIdle() also calls ProcessPendingEvents()
    }
    //else: if we are inside ~wxClipboardSync() and we call ProcessIdle() and
    //      the user app contains an UI update handler which calls wxClipboard::IsSupported,
    //      then we fall into a never-ending loop...

    // put all unprocessed GDK events back in the queue
    GdkDisplay* disp = gtk_widget_get_display(wxGetRootWindow());
    for (size_t i=0; i<m_arrGdkEvents.GetCount(); i++)
    {
        GdkEvent* ev = (GdkEvent*)m_arrGdkEvents[i];

        // NOTE: gdk_display_put_event makes a copy of the event passed to it
        gdk_display_put_event(disp, ev);
        gdk_event_free(ev);
    }

    m_arrGdkEvents.Clear();

#if wxUSE_LOG
    // let the logs be flashed again
    wxLog::Resume();
#endif

    m_isInsideYield = false;

    return true;
}
