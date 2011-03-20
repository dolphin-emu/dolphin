///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/evtloop.h
// Purpose:     wxGTK event loop implementation
// Author:      Vadim Zeitlin
// Created:     2008-12-27
// RCS-ID:      $Id: evtloop.h 62475 2009-10-22 11:36:35Z VZ $
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_EVTLOOP_H_
#define _WX_GTK_EVTLOOP_H_

// ----------------------------------------------------------------------------
// wxGUIEventLoop for wxGTK
// ----------------------------------------------------------------------------

typedef union  _GdkEvent        GdkEvent;

class WXDLLIMPEXP_CORE wxGUIEventLoop : public wxEventLoopBase
{
public:
    wxGUIEventLoop();

    virtual int Run();
    virtual void Exit(int rc = 0);
    virtual bool Pending() const;
    virtual bool Dispatch();
    virtual int DispatchTimeout(unsigned long timeout);
    virtual void WakeUp();
    virtual bool YieldFor(long eventsToProcess);

#if wxUSE_EVENTLOOP_SOURCE
    virtual wxEventLoopSource *
      AddSourceForFD(int fd, wxEventLoopSourceHandler *handler, int flags);
#endif // wxUSE_EVENTLOOP_SOURCE

    void StoreGdkEventForLaterProcessing(GdkEvent* ev)
        { m_arrGdkEvents.Add(ev); }

private:
    // the exit code of this event loop
    int m_exitcode;

    // used to temporarily store events in DoYield()
    wxArrayPtrVoid m_arrGdkEvents;

    wxDECLARE_NO_COPY_CLASS(wxGUIEventLoop);
};

#endif // _WX_GTK_EVTLOOP_H_
