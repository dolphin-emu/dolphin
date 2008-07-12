/////////////////////////////////////////////////////////////////////////////
// Name:        app.h
// Purpose:     wxApp class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: app.h 49563 2007-10-31 20:46:21Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_APP_H_
#define _WX_APP_H_

#include "wx/event.h"
#include "wx/icon.h"

class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxApp;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;
class WXDLLIMPEXP_FWD_BASE wxLog;

// Represents the application. Derive OnInit and declare
// a new App object to start application
class WXDLLEXPORT wxApp : public wxAppBase
{
    DECLARE_DYNAMIC_CLASS(wxApp)

public:
    wxApp();
    virtual ~wxApp();

    // override base class (pure) virtuals
    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp();

    virtual bool Yield(bool onlyIfNeeded = false);
    virtual void WakeUpIdle();

    virtual void SetPrintMode(int mode) { m_printMode = mode; }
    virtual int GetPrintMode() const { return m_printMode; }

    // implementation only
    void OnIdle(wxIdleEvent& event);
    void OnEndSession(wxCloseEvent& event);
    void OnQueryEndSession(wxCloseEvent& event);

#if wxUSE_EXCEPTIONS
    virtual bool OnExceptionInMainLoop();
#endif // wxUSE_EXCEPTIONS

    // deprecated functions, use wxEventLoop directly instead
#if WXWIN_COMPATIBILITY_2_4
    wxDEPRECATED( void DoMessage(WXMSG *pMsg) );
    wxDEPRECATED( bool DoMessage() );
    wxDEPRECATED( bool ProcessMessage(WXMSG* pMsg) );
#endif // WXWIN_COMPATIBILITY_2_4

protected:
    int    m_printMode; // wxPRINT_WINDOWS, wxPRINT_POSTSCRIPT

public:
    // Implementation
    static bool RegisterWindowClasses();
    static bool UnregisterWindowClasses();

#if wxUSE_RICHEDIT
    // initialize the richedit DLL of (at least) given version, return true if
    // ok (Win95 has version 1, Win98/NT4 has 1 and 2, W2K has 3)
    static bool InitRichEdit(int version = 2);
#endif // wxUSE_RICHEDIT

    // returns 400, 470, 471 for comctl32.dll 4.00, 4.70, 4.71 or 0 if it
    // wasn't found at all
    static int GetComCtl32Version();

    // the SW_XXX value to be used for the frames opened by the application
    // (currently seems unused which is a bug -- TODO)
    static int m_nCmdShow;

protected:
    DECLARE_EVENT_TABLE()
    DECLARE_NO_COPY_CLASS(wxApp)
};

// ----------------------------------------------------------------------------
// MSW-specific wxEntry() overload and IMPLEMENT_WXWIN_MAIN definition
// ----------------------------------------------------------------------------

// we need HINSTANCE declaration to define WinMain()
#include "wx/msw/wrapwin.h"

#ifndef SW_SHOWNORMAL
    #define SW_SHOWNORMAL 1
#endif

// WinMain() is always ANSI, even in Unicode build, under normal Windows
// but is always Unicode under CE
#ifdef __WXWINCE__
    typedef wchar_t *wxCmdLineArgType;
#else
    typedef char *wxCmdLineArgType;
#endif

extern int WXDLLEXPORT
wxEntry(HINSTANCE hInstance,
        HINSTANCE hPrevInstance = NULL,
        wxCmdLineArgType pCmdLine = NULL,
        int nCmdShow = SW_SHOWNORMAL);

#define IMPLEMENT_WXWIN_MAIN \
    extern "C" int WINAPI WinMain(HINSTANCE hInstance,                    \
                                  HINSTANCE hPrevInstance,                \
                                  wxCmdLineArgType lpCmdLine,             \
                                  int nCmdShow)                           \
    {                                                                     \
        return wxEntry(hInstance, hPrevInstance, lpCmdLine, nCmdShow);    \
    }

#endif // _WX_APP_H_

