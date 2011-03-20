///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/utilsgui.cpp
// Purpose:     Various utility functions only available in GUI
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.06.2003 (extracted from msw/utils.cpp)
// RCS-ID:      $Id: utilsgui.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Julian Smart
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

#ifndef WX_PRECOMP
    #include "wx/cursor.h"
    #include "wx/window.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/dynlib.h"

#include "wx/msw/private.h"     // includes <windows.h>
#include "wx/msw/registry.h"
#include <shellapi.h> // needed for SHELLEXECUTEINFO


// ============================================================================
// implementation
// ============================================================================

// ---------------------------------------------------------------------------
// helper functions for showing a "busy" cursor
// ---------------------------------------------------------------------------

static HCURSOR gs_wxBusyCursor = 0;     // new, busy cursor
static HCURSOR gs_wxBusyCursorOld = 0;  // old cursor
static int gs_wxBusyCursorCount = 0;

extern HCURSOR wxGetCurrentBusyCursor()
{
    return gs_wxBusyCursor;
}

// Set the cursor to the busy cursor for all windows
void wxBeginBusyCursor(const wxCursor *cursor)
{
    if ( gs_wxBusyCursorCount++ == 0 )
    {
        gs_wxBusyCursor = (HCURSOR)cursor->GetHCURSOR();
#ifndef __WXMICROWIN__
        gs_wxBusyCursorOld = ::SetCursor(gs_wxBusyCursor);
#endif
    }
    //else: nothing to do, already set
}

// Restore cursor to normal
void wxEndBusyCursor()
{
    wxCHECK_RET( gs_wxBusyCursorCount > 0,
                 wxT("no matching wxBeginBusyCursor() for wxEndBusyCursor()") );

    if ( --gs_wxBusyCursorCount == 0 )
    {
#ifndef __WXMICROWIN__
        ::SetCursor(gs_wxBusyCursorOld);
#endif
        gs_wxBusyCursorOld = 0;
    }
}

// true if we're between the above two calls
bool wxIsBusy()
{
  return gs_wxBusyCursorCount > 0;
}

// Check whether this window wants to process messages, e.g. Stop button
// in long calculations.
bool wxCheckForInterrupt(wxWindow *wnd)
{
    wxCHECK( wnd, false );

    MSG msg;
    while ( ::PeekMessage(&msg, GetHwndOf(wnd), 0, 0, PM_REMOVE) )
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return true;
}

// ----------------------------------------------------------------------------
// get display info
// ----------------------------------------------------------------------------

// See also the wxGetMousePosition in window.cpp
// Deprecated: use wxPoint wxGetMousePosition() instead
void wxGetMousePosition( int* x, int* y )
{
    POINT pt;
    GetCursorPos( & pt );
    if ( x ) *x = pt.x;
    if ( y ) *y = pt.y;
}

// Return true if we have a colour display
bool wxColourDisplay()
{
#ifdef __WXMICROWIN__
    // MICROWIN_TODO
    return true;
#else
    // this function is called from wxDC ctor so it is called a *lot* of times
    // hence we optimize it a bit but doing the check only once
    //
    // this should be MT safe as only the GUI thread (holding the GUI mutex)
    // can call us
    static int s_isColour = -1;

    if ( s_isColour == -1 )
    {
        ScreenHDC dc;
        int noCols = ::GetDeviceCaps(dc, NUMCOLORS);

        s_isColour = (noCols == -1) || (noCols > 2);
    }

    return s_isColour != 0;
#endif
}

// Returns depth of screen
int wxDisplayDepth()
{
    ScreenHDC dc;
    return GetDeviceCaps(dc, PLANES) * GetDeviceCaps(dc, BITSPIXEL);
}

// Get size of display
void wxDisplaySize(int *width, int *height)
{
#ifdef __WXMICROWIN__
    RECT rect;
    HWND hWnd = GetDesktopWindow();
    ::GetWindowRect(hWnd, & rect);

    if ( width )
        *width = rect.right - rect.left;
    if ( height )
        *height = rect.bottom - rect.top;
#else // !__WXMICROWIN__
    ScreenHDC dc;

    if ( width )
        *width = ::GetDeviceCaps(dc, HORZRES);
    if ( height )
        *height = ::GetDeviceCaps(dc, VERTRES);
#endif // __WXMICROWIN__/!__WXMICROWIN__
}

void wxDisplaySizeMM(int *width, int *height)
{
#ifdef __WXMICROWIN__
    // MICROWIN_TODO
    if ( width )
        *width = 0;
    if ( height )
        *height = 0;
#else
    ScreenHDC dc;

    if ( width )
        *width = ::GetDeviceCaps(dc, HORZSIZE);
    if ( height )
        *height = ::GetDeviceCaps(dc, VERTSIZE);
#endif
}

void wxClientDisplayRect(int *x, int *y, int *width, int *height)
{
#if defined(__WXMICROWIN__)
    *x = 0; *y = 0;
    wxDisplaySize(width, height);
#else
    // Determine the desktop dimensions minus the taskbar and any other
    // special decorations...
    RECT r;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0);
    if (x)      *x = r.left;
    if (y)      *y = r.top;
    if (width)  *width = r.right - r.left;
    if (height) *height = r.bottom - r.top;
#endif
}

// ---------------------------------------------------------------------------
// window information functions
// ---------------------------------------------------------------------------

wxString WXDLLEXPORT wxGetWindowText(WXHWND hWnd)
{
    wxString str;

    if ( hWnd )
    {
        int len = GetWindowTextLength((HWND)hWnd) + 1;
        ::GetWindowText((HWND)hWnd, wxStringBuffer(str, len), len);
    }

    return str;
}

wxString WXDLLEXPORT wxGetWindowClass(WXHWND hWnd)
{
    wxString str;

    // MICROWIN_TODO
#ifndef __WXMICROWIN__
    if ( hWnd )
    {
        int len = 256; // some starting value

        for ( ;; )
        {
            int count = ::GetClassName((HWND)hWnd, wxStringBuffer(str, len), len);

            if ( count == len )
            {
                // the class name might have been truncated, retry with larger
                // buffer
                len *= 2;
            }
            else
            {
                break;
            }
        }
    }
#endif // !__WXMICROWIN__

    return str;
}

int WXDLLEXPORT wxGetWindowId(WXHWND hWnd)
{
    return ::GetWindowLong((HWND)hWnd, GWL_ID);
}

// ----------------------------------------------------------------------------
// Metafile helpers
// ----------------------------------------------------------------------------

void PixelToHIMETRIC(LONG *x, LONG *y, HDC hdcRef)
{
    int iWidthMM = GetDeviceCaps(hdcRef, HORZSIZE),
        iHeightMM = GetDeviceCaps(hdcRef, VERTSIZE),
        iWidthPels = GetDeviceCaps(hdcRef, HORZRES),
        iHeightPels = GetDeviceCaps(hdcRef, VERTRES);

    *x *= (iWidthMM * 100);
    *x /= iWidthPels;
    *y *= (iHeightMM * 100);
    *y /= iHeightPels;
}

void HIMETRICToPixel(LONG *x, LONG *y, HDC hdcRef)
{
    int iWidthMM = GetDeviceCaps(hdcRef, HORZSIZE),
        iHeightMM = GetDeviceCaps(hdcRef, VERTSIZE),
        iWidthPels = GetDeviceCaps(hdcRef, HORZRES),
        iHeightPels = GetDeviceCaps(hdcRef, VERTRES);

    *x *= iWidthPels;
    *x /= (iWidthMM * 100);
    *y *= iHeightPels;
    *y /= (iHeightMM * 100);
}

void HIMETRICToPixel(LONG *x, LONG *y)
{
    HIMETRICToPixel(x, y, ScreenHDC());
}

void PixelToHIMETRIC(LONG *x, LONG *y)
{
    PixelToHIMETRIC(x, y, ScreenHDC());
}

void wxDrawLine(HDC hdc, int x1, int y1, int x2, int y2)
{
#ifdef __WXWINCE__
    POINT points[2];
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    Polyline(hdc, points, 2);
#else
    MoveToEx(hdc, x1, y1, NULL); LineTo((HDC) hdc, x2, y2);
#endif
}


// ----------------------------------------------------------------------------
// Shell API wrappers
// ----------------------------------------------------------------------------

extern bool wxEnableFileNameAutoComplete(HWND hwnd)
{
#if wxUSE_DYNLIB_CLASS
    typedef HRESULT (WINAPI *SHAutoComplete_t)(HWND, DWORD);

    static SHAutoComplete_t s_pfnSHAutoComplete = NULL;
    static bool s_initialized = false;

    if ( !s_initialized )
    {
        s_initialized = true;

        wxLogNull nolog;
        wxDynamicLibrary dll(wxT("shlwapi.dll"));
        if ( dll.IsLoaded() )
        {
            s_pfnSHAutoComplete =
                (SHAutoComplete_t)dll.GetSymbol(wxT("SHAutoComplete"));
            if ( s_pfnSHAutoComplete )
            {
                // won't be unloaded until the process termination, no big deal
                dll.Detach();
            }
        }
    }

    if ( !s_pfnSHAutoComplete )
        return false;

    HRESULT hr = s_pfnSHAutoComplete(hwnd, 0x10 /* SHACF_FILESYS_ONLY */);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxT("SHAutoComplete"), hr);
        return false;
    }

    return true;
#else
    wxUnusedVar(hwnd);
    return false;
#endif // wxUSE_DYNLIB_CLASS/!wxUSE_DYNLIB_CLASS
}

// ----------------------------------------------------------------------------
// Launch document with default app
// ----------------------------------------------------------------------------

bool wxLaunchDefaultApplication(const wxString& document, int flags)
{
    wxUnusedVar(flags);

    WinStruct<SHELLEXECUTEINFO> sei;
    sei.lpFile = document.wx_str();
#ifdef __WXWINCE__
    sei.nShow = SW_SHOWNORMAL; // SW_SHOWDEFAULT not defined under CE (#10216)
#else
    sei.nShow = SW_SHOWDEFAULT;
#endif

    // avoid Windows message box in case of error for consistency with
    // wxLaunchDefaultBrowser() even if don't show the error ourselves in this
    // function
    sei.fMask = SEE_MASK_FLAG_NO_UI;

    if ( ::ShellExecuteEx(&sei) )
        return true;

    return false;
}

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

bool wxDoLaunchDefaultBrowser(const wxString& url, const wxString& scheme, int flags)
{
    wxUnusedVar(flags);

#if wxUSE_IPC
    if ( flags & wxBROWSER_NEW_WINDOW )
    {
        // ShellExecuteEx() opens the URL in an existing window by default so
        // we can't use it if we need a new window
        wxRegKey key(wxRegKey::HKCR, scheme + wxT("\\shell\\open"));
        if ( !key.Exists() )
        {
            // try the default browser, it must be registered at least for http URLs
            key.SetName(wxRegKey::HKCR, wxT("http\\shell\\open"));
        }

        if ( key.Exists() )
        {
            wxRegKey keyDDE(key, wxT("DDEExec"));
            if ( keyDDE.Exists() )
            {
                // we only know the syntax of WWW_OpenURL DDE request for IE,
                // optimistically assume that all other browsers are compatible
                // with it
                static const wxChar *TOPIC_OPEN_URL = wxT("WWW_OpenURL");
                wxString ddeCmd;
                wxRegKey keyTopic(keyDDE, wxT("topic"));
                bool ok = keyTopic.Exists() &&
                            keyTopic.QueryDefaultValue() == TOPIC_OPEN_URL;
                if ( ok )
                {
                    ddeCmd = keyDDE.QueryDefaultValue();
                    ok = !ddeCmd.empty();
                }

                if ( ok )
                {
                    // for WWW_OpenURL, the index of the window to open the URL
                    // in is -1 (meaning "current") by default, replace it with
                    // 0 which means "new" (see KB article 160957)
                    ok = ddeCmd.Replace(wxT("-1"), wxT("0"),
                                        false /* only first occurrence */) == 1;
                }

                if ( ok )
                {
                    // and also replace the parameters: the topic should
                    // contain a placeholder for the URL
                    ok = ddeCmd.Replace(wxT("%1"), url, false) == 1;
                }

                if ( ok )
                {
                    // try to send it the DDE request now but ignore the errors
                    wxLogNull noLog;

                    const wxString ddeServer = wxRegKey(keyDDE, wxT("application"));
                    if ( wxExecuteDDE(ddeServer, TOPIC_OPEN_URL, ddeCmd) )
                        return true;

                    // this is not necessarily an error: maybe browser is
                    // simply not running, but no matter, in any case we're
                    // going to launch it using ShellExecuteEx() below now and
                    // we shouldn't try to open a new window if we open a new
                    // browser anyhow
                }
            }
        }
    }
#endif // wxUSE_IPC

    WinStruct<SHELLEXECUTEINFO> sei;
    sei.lpFile = url.c_str();
    sei.lpVerb = wxT("open");
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_FLAG_NO_UI; // we give error message ourselves

    if ( ::ShellExecuteEx(&sei) )
        return true;

    return false;
}
