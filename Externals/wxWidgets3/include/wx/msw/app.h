/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/app.h
// Purpose:     wxApp class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: app.h 67254 2011-03-20 00:14:35Z DS $
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
class WXDLLIMPEXP_CORE wxApp : public wxAppBase
{
public:
    wxApp();
    virtual ~wxApp();

    // override base class (pure) virtuals
    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp();

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

    // MSW-specific from now on
    // ------------------------

    // this suffix should be appended to all our Win32 class names to obtain a
    // variant registered without CS_[HV]REDRAW styles
    static const wxChar *GetNoRedrawClassSuffix() { return wxT("NR"); }

    // get the name of the registered Win32 class with the given (unique) base
    // name: this function constructs the unique class name using this name as
    // prefix, checks if the class is already registered and registers it if it
    // isn't and returns the name it was registered under (or NULL if it failed)
    //
    // the registered class will always have CS_[HV]REDRAW and CS_DBLCLKS
    // styles as well as any additional styles specified as arguments here; and
    // there will be also a companion registered class identical to this one
    // but without CS_[HV]REDRAW whose name will be the same one but with
    // GetNoRedrawClassSuffix()
    //
    // the background brush argument must be either a COLOR_XXX standard value
    // or (default) -1 meaning that the class paints its background itself
    static const wxChar *GetRegisteredClassName(const wxChar *name,
                                                int bgBrushCol = -1,
                                                int extraStyles = 0);

    // return true if this name corresponds to one of the classes we registered
    // in the previous GetRegisteredClassName() calls
    static bool IsRegisteredClassName(const wxString& name);

protected:
    int    m_printMode; // wxPRINT_WINDOWS, wxPRINT_POSTSCRIPT

public:
    // unregister any window classes registered by GetRegisteredClassName()
    static void UnregisterWindowClasses();

#if wxUSE_RICHEDIT
    // initialize the richedit DLL of (at least) given version, return true if
    // ok (Win95 has version 1, Win98/NT4 has 1 and 2, W2K has 3)
    static bool InitRichEdit(int version = 2);
#endif // wxUSE_RICHEDIT

    // returns 400, 470, 471 for comctl32.dll 4.00, 4.70, 4.71 or 0 if it
    // wasn't found at all
    static int GetComCtl32Version();

    // the same for shell32.dll: returns 400, 471, 500, 600, ... (4.70 not
    // currently detected)
    static int GetShell32Version();

    // the SW_XXX value to be used for the frames opened by the application
    // (currently seems unused which is a bug -- TODO)
    static int m_nCmdShow;

protected:
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxApp);
    DECLARE_DYNAMIC_CLASS(wxApp)
};

#ifdef __WXWINCE__

// under CE provide a dummy implementation of GetComCtl32Version() returning
// the value passing all ">= 470" tests (which are the only ones used in our
// code currently) as commctrl.dll under CE 2.0 and later support comctl32.dll
// functionality
inline int wxApp::GetComCtl32Version()
{
    return 471;
}

// this is not currently used at all under CE so it's not really clear what do
// we need to return from here
inline int wxApp::GetShell32Version()
{
    return 0;
}

#endif // __WXWINCE__

// ----------------------------------------------------------------------------
// MSW-specific wxEntry() overload and wxIMPLEMENT_WXWIN_MAIN definition
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

// wxMSW-only overloads of wxEntry() and wxEntryStart() which take the
// parameters passed to WinMain() instead of those passed to main()
extern WXDLLIMPEXP_CORE bool
    wxEntryStart(HINSTANCE hInstance,
                HINSTANCE hPrevInstance = NULL,
                wxCmdLineArgType pCmdLine = NULL,
                int nCmdShow = SW_SHOWNORMAL);

extern WXDLLIMPEXP_CORE int
    wxEntry(HINSTANCE hInstance,
            HINSTANCE hPrevInstance = NULL,
            wxCmdLineArgType pCmdLine = NULL,
            int nCmdShow = SW_SHOWNORMAL);

#if defined(__BORLANDC__) && wxUSE_UNICODE
    // Borland C++ has the following nonstandard behaviour: when the -WU
    // command line flag is used, the linker expects to find wWinMain instead
    // of WinMain. This flag causes the compiler to define _UNICODE and
    // UNICODE symbols and there's no way to detect its use, so we have to
    // define both WinMain and wWinMain so that wxIMPLEMENT_WXWIN_MAIN works
    // for both code compiled with and without -WU.
    // See http://sourceforge.net/tracker/?func=detail&atid=309863&aid=1935997&group_id=9863
    // for more details.
    #define wxIMPLEMENT_WXWIN_MAIN_BORLAND_NONSTANDARD                      \
        extern "C" int WINAPI wWinMain(HINSTANCE hInstance,                 \
                                      HINSTANCE hPrevInstance,              \
                                      wchar_t * WXUNUSED(lpCmdLine),        \
                                      int nCmdShow)                         \
        {                                                                   \
            wxDISABLE_DEBUG_SUPPORT();                                      \
                                                                            \
            /* NB: wxEntry expects lpCmdLine argument to be char*, not */   \
            /*     wchar_t*, but fortunately it's not used anywhere    */   \
            /*     and we can simply pass NULL in:                     */   \
            return wxEntry(hInstance, hPrevInstance, NULL, nCmdShow);       \
        }
#else
    #define wxIMPLEMENT_WXWIN_MAIN_BORLAND_NONSTANDARD
#endif // defined(__BORLANDC__) && wxUSE_UNICODE

#define wxIMPLEMENT_WXWIN_MAIN                                              \
    extern "C" int WINAPI WinMain(HINSTANCE hInstance,                      \
                                  HINSTANCE hPrevInstance,                  \
                                  wxCmdLineArgType WXUNUSED(lpCmdLine),     \
                                  int nCmdShow)                             \
    {                                                                       \
        wxDISABLE_DEBUG_SUPPORT();                                          \
                                                                            \
        /* NB: We pass NULL in place of lpCmdLine to behave the same as  */ \
        /*     Borland-specific wWinMain() above. If it becomes needed   */ \
        /*     to pass lpCmdLine to wxEntry() here, you'll have to fix   */ \
        /*     wWinMain() above too.                                     */ \
        return wxEntry(hInstance, hPrevInstance, NULL, nCmdShow);           \
    }                                                                       \
    wxIMPLEMENT_WXWIN_MAIN_BORLAND_NONSTANDARD


#endif // _WX_APP_H_

