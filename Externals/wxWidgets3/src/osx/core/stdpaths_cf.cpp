///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/stdpaths_cf.cpp
// Purpose:     wxStandardPaths implementation for CoreFoundation systems
// Author:      David Elliott
// Modified by:
// Created:     2004-10-27
// RCS-ID:      $Id: stdpaths_cf.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 2004 David Elliott <dfe@cox.net>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if wxUSE_STDPATHS

#ifndef WX_PRECOMP
    #include "wx/intl.h"
#endif //ndef WX_PRECOMP

#include "wx/stdpaths.h"
#include "wx/filename.h"
#ifdef __WXMAC__
#include "wx/osx/private.h"
#endif
#include "wx/osx/core/cfstring.h"

#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURL.h>

#define kDefaultPathStyle kCFURLPOSIXPathStyle

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxStandardPathsCF ctors/dtor
// ----------------------------------------------------------------------------

wxStandardPathsCF::wxStandardPathsCF()
                 : m_bundle(CFBundleGetMainBundle())
{
    CFRetain(m_bundle);
}

wxStandardPathsCF::wxStandardPathsCF(wxCFBundleRef bundle)
                 : m_bundle(bundle)
{
    CFRetain(m_bundle);
}

wxStandardPathsCF::~wxStandardPathsCF()
{
    CFRelease(m_bundle);
}

// ----------------------------------------------------------------------------
// wxStandardPathsCF Mac-specific methods
// ----------------------------------------------------------------------------

void wxStandardPathsCF::SetBundle(wxCFBundleRef bundle)
{
    CFRetain(bundle);
    CFRelease(m_bundle);
    m_bundle = bundle;
}

// ----------------------------------------------------------------------------
// generic functions in terms of which the other ones are implemented
// ----------------------------------------------------------------------------

static wxString BundleRelativeURLToPath(CFURLRef relativeURL)
{
    CFURLRef absoluteURL = CFURLCopyAbsoluteURL(relativeURL);
    wxCHECK_MSG(absoluteURL, wxEmptyString, wxT("Failed to resolve relative URL to absolute URL"));
    CFStringRef cfStrPath = CFURLCopyFileSystemPath(absoluteURL,kDefaultPathStyle);
    CFRelease(absoluteURL);
    return wxCFStringRef(cfStrPath).AsString(wxLocale::GetSystemEncoding());
}

wxString wxStandardPathsCF::GetFromFunc(wxCFURLRef (*func)(wxCFBundleRef)) const
{
    wxCHECK_MSG(m_bundle, wxEmptyString,
                wxT("wxStandardPaths for CoreFoundation only works with bundled apps"));
    CFURLRef relativeURL = (*func)(m_bundle);
    wxCHECK_MSG(relativeURL, wxEmptyString, wxT("Couldn't get URL"));
    wxString ret(BundleRelativeURLToPath(relativeURL));
    CFRelease(relativeURL);
    return ret;
}

wxString wxStandardPathsCF::GetDocumentsDir() const
{
#if defined( __WXMAC__ ) && wxOSX_USE_CARBON
    return wxMacFindFolderNoSeparator
        (
        kUserDomain,
        kDocumentsFolderType,
        kCreateFolder
        );
#else
    return wxFileName::GetHomeDir() + wxT("/Documents");
#endif
}

// ----------------------------------------------------------------------------
// wxStandardPathsCF public API
// ----------------------------------------------------------------------------

wxString wxStandardPathsCF::GetConfigDir() const
{
#if defined( __WXMAC__ ) && wxOSX_USE_CARBON
    return wxMacFindFolderNoSeparator((short)kLocalDomain, kPreferencesFolderType, kCreateFolder);
#else
    return wxT("/Library/Preferences");
#endif
}

wxString wxStandardPathsCF::GetUserConfigDir() const
{
#if defined( __WXMAC__ ) && wxOSX_USE_CARBON
    return wxMacFindFolderNoSeparator((short)kUserDomain, kPreferencesFolderType, kCreateFolder);
#else
    return wxFileName::GetHomeDir() + wxT("/Library/Preferences");
#endif
}

wxString wxStandardPathsCF::GetDataDir() const
{
    return GetFromFunc(CFBundleCopySharedSupportURL);
}

wxString wxStandardPathsCF::GetExecutablePath() const
{
#ifdef __WXMAC__
    return GetFromFunc(CFBundleCopyExecutableURL);
#else
    return wxStandardPathsBase::GetExecutablePath();
#endif
}

wxString wxStandardPathsCF::GetLocalDataDir() const
{
#if defined( __WXMAC__ ) && wxOSX_USE_CARBON
    return AppendAppInfo(wxMacFindFolderNoSeparator((short)kLocalDomain, kApplicationSupportFolderType, kCreateFolder));
#else
    return AppendAppInfo(wxT("/Library/Application Support"));
#endif
}

wxString wxStandardPathsCF::GetUserDataDir() const
{
#if defined( __WXMAC__ ) && wxOSX_USE_CARBON
    return AppendAppInfo(wxMacFindFolderNoSeparator((short)kUserDomain, kApplicationSupportFolderType, kCreateFolder));
#else
    return AppendAppInfo(wxFileName::GetHomeDir() + wxT("/Library/Application Support"));
#endif
}

wxString wxStandardPathsCF::GetPluginsDir() const
{
    return GetFromFunc(CFBundleCopyBuiltInPlugInsURL);
}

wxString wxStandardPathsCF::GetResourcesDir() const
{
    return GetFromFunc(CFBundleCopyResourcesDirectoryURL);
}

wxString
wxStandardPathsCF::GetLocalizedResourcesDir(const wxString& lang,
                                            ResourceCat category) const
{
    return wxStandardPathsBase::
            GetLocalizedResourcesDir(lang, category) + wxT(".lproj");
}

#endif // wxUSE_STDPATHS
