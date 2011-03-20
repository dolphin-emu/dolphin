///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/apptrait.h
// Purpose:     class implementing wxAppTraits for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.06.2003
// RCS-ID:      $Id: apptrait.h 67185 2011-03-14 11:54:32Z VZ $
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_APPTRAIT_H_
#define _WX_MSW_APPTRAIT_H_

// ----------------------------------------------------------------------------
// wxGUI/ConsoleAppTraits: must derive from wxAppTraits, not wxAppTraitsBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxConsoleAppTraits : public wxConsoleAppTraitsBase
{
public:
    virtual wxEventLoopBase *CreateEventLoop();
    virtual void *BeforeChildWaitLoop();
    virtual void AfterChildWaitLoop(void *data);
#if wxUSE_TIMER
    virtual wxTimerImpl *CreateTimerImpl(wxTimer *timer);
#endif
    virtual bool DoMessageFromThreadWait();
    virtual WXDWORD WaitForThread(WXHANDLE hThread, int flags);
#ifndef __WXWINCE__
    virtual bool CanUseStderr() { return true; }
    virtual bool WriteToStderr(const wxString& text);
#endif // !__WXWINCE__
};

#if wxUSE_GUI

class WXDLLIMPEXP_CORE wxGUIAppTraits : public wxGUIAppTraitsBase
{
public:
    virtual wxEventLoopBase *CreateEventLoop();
    virtual void *BeforeChildWaitLoop();
    virtual void AfterChildWaitLoop(void *data);
#if wxUSE_TIMER
    virtual wxTimerImpl *CreateTimerImpl(wxTimer *timer);
#endif
    virtual bool DoMessageFromThreadWait();
    virtual wxPortId GetToolkitVersion(int *majVer = NULL, int *minVer = NULL) const;
    virtual WXDWORD WaitForThread(WXHANDLE hThread, int flags);

#ifndef __WXWINCE__
    virtual bool CanUseStderr();
    virtual bool WriteToStderr(const wxString& text);
#endif // !__WXWINCE__
};

#endif // wxUSE_GUI

#endif // _WX_MSW_APPTRAIT_H_
