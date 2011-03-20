///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/cocoa/evtloop.h
// Purpose:     declaration of wxGUIEventLoop for wxOSX/Cocoa
// Author:      Vadim Zeitlin
// Created:     2008-12-28
// RCS-ID:      $Id: evtloop.h 67129 2011-03-05 12:21:20Z SC $
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_COCOA_EVTLOOP_H_
#define _WX_OSX_COCOA_EVTLOOP_H_

class WXDLLIMPEXP_BASE wxGUIEventLoop : public wxCFEventLoop
{
public:
    wxGUIEventLoop();
    ~wxGUIEventLoop();
    
    void BeginModalSession( wxWindow* modalWindow );
    
    void EndModalSession();

protected:
    virtual int DoDispatchTimeout(unsigned long timeout);

    virtual void DoRun();

    virtual void DoStop();

    virtual CFRunLoopRef CFGetCurrentRunLoop() const;
    
    void* m_modalSession;
    WXWindow m_dummyWindow;
};

#endif // _WX_OSX_COCOA_EVTLOOP_H_

