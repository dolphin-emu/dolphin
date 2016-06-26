/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/stdpaths.mm
// Purpose:     wxStandardPaths for Cocoa
// Author:      Tobias Taschner
// Created:     2015-09-09
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if wxUSE_STDPATHS

#include "wx/stdpaths.h"
#include "wx/osx/private.h"
#include "wx/osx/core/cfstring.h"

#import <CoreFoundation/CoreFoundation.h>

// ============================================================================
// implementation
// ============================================================================

static wxString GetFMDirectory(
                                   NSSearchPathDirectory directory,
                                   NSSearchPathDomainMask domainMask)
{
    NSURL* url = [[NSFileManager defaultManager] URLForDirectory:directory
                                           inDomain:domainMask
                                  appropriateForURL:nil
                                             create:NO error:nil];
    return wxCFStringRef::AsString((CFStringRef)url.path);
}

wxStandardPaths::wxStandardPaths()
{

}

wxStandardPaths::~wxStandardPaths()
{

}

wxString wxStandardPaths::GetExecutablePath() const
{
    return wxCFStringRef::AsString((CFStringRef)[NSBundle mainBundle].executablePath);
}

wxString wxStandardPaths::GetConfigDir() const
{
    return GetFMDirectory(NSLibraryDirectory, NSLocalDomainMask) + "/Preferences";
}

wxString wxStandardPaths::GetUserConfigDir() const
{
    return GetFMDirectory(NSLibraryDirectory, NSUserDomainMask) + "/Preferences";
}

wxString wxStandardPaths::GetDataDir() const
{
    return wxCFStringRef::AsString((CFStringRef)[NSBundle mainBundle].sharedSupportPath);
}

wxString wxStandardPaths::GetLocalDataDir() const
{
    return AppendAppInfo(GetFMDirectory(NSApplicationSupportDirectory, NSLocalDomainMask));
}

wxString wxStandardPaths::GetUserDataDir() const
{
    return AppendAppInfo(GetFMDirectory(NSApplicationSupportDirectory, NSUserDomainMask));
}

wxString wxStandardPaths::GetPluginsDir() const
{
    return wxCFStringRef::AsString((CFStringRef)[NSBundle mainBundle].builtInPlugInsPath);
}

wxString wxStandardPaths::GetResourcesDir() const
{
    return wxCFStringRef::AsString((CFStringRef)[NSBundle mainBundle].resourcePath);
}

wxString
wxStandardPaths::GetLocalizedResourcesDir(const wxString& lang,
                                          ResourceCat category) const
{
    return wxStandardPathsBase::
        GetLocalizedResourcesDir(lang, category) + wxT(".lproj");
}

wxString wxStandardPaths::GetUserDir(Dir userDir) const
{
    NSSearchPathDirectory dirType;
    switch (userDir)
    {
        case Dir_Desktop:
            dirType = NSDesktopDirectory;
            break;
        case Dir_Downloads:
            dirType = NSDownloadsDirectory;
            break;
        case Dir_Music:
            dirType = NSMusicDirectory;
            break;
        case Dir_Pictures:
            dirType = NSPicturesDirectory;
            break;
        case Dir_Videos:
            dirType = NSMoviesDirectory;
            break;
        default:
            dirType = NSDocumentDirectory;
            break;
    }

    return GetFMDirectory(dirType, NSUserDomainMask);
}

#endif // wxUSE_STDPATHS
