///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/stdpaths.cpp
// Purpose:     wxStandardPaths implementation for Win32
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-10-19
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
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

#if wxUSE_STDPATHS

#include "wx/stdpaths.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/dynlib.h"
#include "wx/filename.h"

#include "wx/msw/private.h"
#include "wx/msw/wrapshl.h"
#include <initguid.h>

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

typedef HRESULT (WINAPI *SHGetKnownFolderPath_t)(const GUID&, DWORD, HANDLE, PWSTR *);

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// used in our wxLogTrace messages
#define TRACE_MASK wxT("stdpaths")

#ifndef CSIDL_APPDATA
    #define CSIDL_APPDATA         0x001a
#endif

#ifndef CSIDL_LOCAL_APPDATA
    #define CSIDL_LOCAL_APPDATA   0x001c
#endif

#ifndef CSIDL_COMMON_APPDATA
    #define CSIDL_COMMON_APPDATA  0x0023
#endif

#ifndef CSIDL_PROGRAM_FILES
    #define CSIDL_PROGRAM_FILES   0x0026
#endif

#ifndef CSIDL_PERSONAL
    #define CSIDL_PERSONAL        0x0005
#endif

#ifndef SHGFP_TYPE_CURRENT
    #define SHGFP_TYPE_CURRENT 0
#endif

#ifndef SHGFP_TYPE_DEFAULT
    #define SHGFP_TYPE_DEFAULT 1
#endif

// ----------------------------------------------------------------------------
// module globals
// ----------------------------------------------------------------------------

namespace
{

DEFINE_GUID(wxFOLDERID_Downloads,
    0x374de290, 0x123f, 0x4565, 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b);

struct ShellFunctions
{
    ShellFunctions()
    {
        pSHGetKnownFolderPath = NULL;
        initialized = false;
    }

    SHGetKnownFolderPath_t pSHGetKnownFolderPath;

    bool initialized;
};

// in spite of using a static variable, this is MT-safe as in the worst case it
// results in initializing the function pointer several times -- but this is
// harmless
ShellFunctions gs_shellFuncs;

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

void ResolveShellFunctions()
{
#if wxUSE_DYNLIB_CLASS

    // start with the newest functions, fall back to the oldest ones
    // first check for SHGetFolderPath (shell32.dll 5.0)
    wxString shellDllName(wxT("shell32"));

    wxDynamicLibrary dllShellFunctions( shellDllName );
    if ( !dllShellFunctions.IsLoaded() )
    {
        wxLogTrace(TRACE_MASK, wxT("Failed to load %s.dll"), shellDllName.c_str() );
    }

    // don't give errors if the functions are unavailable, we're ready to deal
    // with this
    wxLogNull noLog;

    gs_shellFuncs.pSHGetKnownFolderPath = (SHGetKnownFolderPath_t)
        dllShellFunctions.GetSymbol("SHGetKnownFolderPath");

    // shell32.dll is going to be unloaded, but it still remains in memory
    // because we also link to it statically, so it's ok

    gs_shellFuncs.initialized = true;
#endif
}

} // anonymous namespace

// ============================================================================
// wxStandardPaths implementation
// ============================================================================

// ----------------------------------------------------------------------------
// private helpers
// ----------------------------------------------------------------------------

/* static */
wxString wxStandardPaths::DoGetDirectory(int csidl)
{
    wxString dir;
    HRESULT hr = E_FAIL;

    hr = ::SHGetFolderPath
            (
            NULL,               // parent window, not used
            csidl,
            NULL,               // access token (current user)
            SHGFP_TYPE_CURRENT, // current path, not just default value
            wxStringBuffer(dir, MAX_PATH)
            );

    // somewhat incredibly, the error code in the Unicode version is
    // different from the one in ASCII version for this function
#if wxUSE_UNICODE
    if ( hr == E_FAIL )
#else
    if ( hr == S_FALSE )
#endif
    {
        // directory doesn't exist, maybe we can get its default value?
        hr = ::SHGetFolderPath
                (
                NULL,
                csidl,
                NULL,
                SHGFP_TYPE_DEFAULT,
                wxStringBuffer(dir, MAX_PATH)
                );
    }

    return dir;
}

wxString wxStandardPaths::DoGetKnownFolder(const GUID& rfid)
{
    if (!gs_shellFuncs.initialized)
        ResolveShellFunctions();

    wxString dir;

    if ( gs_shellFuncs.pSHGetKnownFolderPath )
    {
        PWSTR pDir;
        HRESULT hr = gs_shellFuncs.pSHGetKnownFolderPath(rfid, 0, 0, &pDir);
        if ( SUCCEEDED(hr) )
        {
            dir = pDir;
            CoTaskMemFree(pDir);
        }
    }

    return dir;
}


wxString wxStandardPaths::GetAppDir() const
{
    if ( m_appDir.empty() )
    {
        m_appDir = wxFileName(wxGetFullModuleName()).GetPath();
    }

    return m_appDir;
}

wxString wxStandardPaths::GetUserDir(Dir userDir) const
{
    int csidl;
    switch (userDir)
    {
        case Dir_Desktop:
            csidl = CSIDL_DESKTOPDIRECTORY;
            break;
        case Dir_Downloads:
        {
            csidl = CSIDL_PERSONAL;
            // Downloads folder is only available since Vista
            wxString dir = DoGetKnownFolder(wxFOLDERID_Downloads);
            if ( !dir.empty() )
                return dir;
            break;
        }
        case Dir_Music:
            csidl = CSIDL_MYMUSIC;
            break;
        case Dir_Pictures:
            csidl = CSIDL_MYPICTURES;
            break;
        case Dir_Videos:
            csidl = CSIDL_MYVIDEO;
            break;
        default:
            csidl = CSIDL_PERSONAL;
            break;
    }

    return DoGetDirectory(csidl);
}

// ----------------------------------------------------------------------------
// MSW-specific functions
// ----------------------------------------------------------------------------

void wxStandardPaths::IgnoreAppSubDir(const wxString& subdirPattern)
{
    wxFileName fn = wxFileName::DirName(GetAppDir());

    if ( !fn.GetDirCount() )
    {
        // no last directory to ignore anyhow
        return;
    }

    const wxString lastdir = fn.GetDirs().Last().Lower();
    if ( lastdir.Matches(subdirPattern.Lower()) )
    {
        fn.RemoveLastDir();

        // store the cached value so that subsequent calls to GetAppDir() will
        // reuse it instead of using just the program binary directory
        m_appDir = fn.GetPath();
    }
}

void wxStandardPaths::IgnoreAppBuildSubDirs()
{
    IgnoreAppSubDir("debug");
    IgnoreAppSubDir("release");

    // there can also be an architecture-dependent parent directory, ignore it
    // as well
#ifdef __WIN64__
    IgnoreAppSubDir("x64");
#else // __WIN32__
    IgnoreAppSubDir("Win32");
#endif // __WIN64__/__WIN32__

    wxString compilerPrefix;
#ifdef __VISUALC__
    compilerPrefix = "vc";
#elif defined(__GNUG__)
    compilerPrefix = "gcc";
#elif defined(__BORLANDC__)
    compilerPrefix = "bcc";
#else
    return;
#endif

    IgnoreAppSubDir(compilerPrefix + "_msw*");
}

void wxStandardPaths::DontIgnoreAppSubDir()
{
    // this will force the next call to GetAppDir() to use the program binary
    // path as the application directory
    m_appDir.clear();
}

/* static */
wxString wxStandardPaths::MSWGetShellDir(int csidl)
{
    return DoGetDirectory(csidl);
}

// ----------------------------------------------------------------------------
// public functions
// ----------------------------------------------------------------------------

wxStandardPaths::wxStandardPaths()
{
    // make it possible to run uninstalled application from the build directory
    IgnoreAppBuildSubDirs();
}

wxString wxStandardPaths::GetExecutablePath() const
{
    return wxGetFullModuleName();
}

wxString wxStandardPaths::GetConfigDir() const
{
    return AppendAppInfo(DoGetDirectory(CSIDL_COMMON_APPDATA));
}

wxString wxStandardPaths::GetUserConfigDir() const
{
    return DoGetDirectory(CSIDL_APPDATA);
}

wxString wxStandardPaths::GetDataDir() const
{
    // under Windows each program is usually installed in its own directory and
    // so its datafiles are in the same directory as its main executable
    return GetAppDir();
}

wxString wxStandardPaths::GetUserDataDir() const
{
    return AppendAppInfo(GetUserConfigDir());
}

wxString wxStandardPaths::GetUserLocalDataDir() const
{
    return AppendAppInfo(DoGetDirectory(CSIDL_LOCAL_APPDATA));
}

wxString wxStandardPaths::GetPluginsDir() const
{
    // there is no standard location for plugins, suppose they're in the same
    // directory as the .exe
    return GetAppDir();
}

// ============================================================================
// wxStandardPathsWin16 implementation
// ============================================================================

wxString wxStandardPathsWin16::GetConfigDir() const
{
    // this is for compatibility with earlier wxFileConfig versions
    // which used the Windows directory for the global files
    wxString dir;
    if ( !::GetWindowsDirectory(wxStringBuffer(dir, MAX_PATH), MAX_PATH) )
    {
        wxLogLastError(wxT("GetWindowsDirectory"));
    }

    return dir;
}

wxString wxStandardPathsWin16::GetUserConfigDir() const
{
    // again, for wxFileConfig which uses $HOME for its user config file
    return wxGetHomeDir();
}

#endif // wxUSE_STDPATHS
