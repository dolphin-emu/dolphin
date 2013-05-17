///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/evtloop.h
// Purpose:     declaration of wxEventLoop for wxMac
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2006-01-12
// RCS-ID:      $Id: evtloop.h 68302 2011-07-19 17:56:57Z SC $
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_CARBON_EVTLOOP_H_
#define _WX_MAC_CARBON_EVTLOOP_H_

struct OpaqueEventRef;
typedef OpaqueEventRef *EventRef;

class WXDLLIMPEXP_CORE wxGUIEventLoop : public wxCFEventLoop
{
public:
    wxGUIEventLoop();

    virtual void WakeUp();

protected:
    virtual int DoDispatchTimeout(unsigned long timeout);

    virtual void DoRun();

    virtual void DoStop();

    virtual CFRunLoopRef CFGetCurrentRunLoop() const;
};

#endif // _WX_MAC_CARBON_EVTLOOP_H_

