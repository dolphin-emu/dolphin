/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/utilsexc_base.cpp
// Purpose:     wxMacLaunch
// Author:      Ryan Norton
// Modified by:
// Created:     2005-06-21
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
// Notes:       Source was originally in utilsexc_cf.cpp,1.6 then moved
//              to totally unrelated hid.cpp,1.8.
/////////////////////////////////////////////////////////////////////////////

//===========================================================================
//  DECLARATIONS
//===========================================================================

//---------------------------------------------------------------------------
// Pre-compiled header stuff
//---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

// WX includes
#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/utils.h"
    #include "wx/wxcrt.h"
#endif // WX_PRECOMP

// Mac Includes
#include <CoreFoundation/CoreFoundation.h>
#ifndef __WXOSX_IPHONE__
#include <ApplicationServices/ApplicationServices.h>
#endif

// More WX Includes
#include "wx/filename.h"
#include "wx/osx/core/cfstring.h"
#include "wx/osx/core/private.h"

// Default path style
#define kDefaultPathStyle kCFURLPOSIXPathStyle

#if wxUSE_SOCKETS
// global pointer which lives in the base library, set from the net one (see
// sockosx.cpp) and used from the GUI code (see utilsexc_cf.cpp) -- ugly but
// needed hack, see the above-mentioned files for more information
class wxSocketManager;
extern WXDLLIMPEXP_BASE wxSocketManager *wxOSXSocketManagerCF;
wxSocketManager *wxOSXSocketManagerCF = NULL;
#endif // wxUSE_SOCKETS

#if ( !wxUSE_GUI && !wxOSX_USE_IPHONE ) || wxOSX_USE_COCOA_OR_CARBON

//===========================================================================
//  IMPLEMENTATION
//===========================================================================

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//    wxMacLaunch
//
// argv is the command line split up, with the application path first
// flags are the flags from wxExecute
// process is the process passed from wxExecute for pipe streams etc.
// returns -1 on error for wxEXEC_SYNC and 0 on error for wxEXEC_ASYNC
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool wxMacLaunch(char **argv)
{
    // Obtains the number of arguments for determining the size of
    // the CFArray used to hold them
    CFIndex cfiCount = 0;
    for(char** argvcopy = argv; *argvcopy != NULL ; ++argvcopy)
    {
        ++cfiCount;
    }

    // If there is not a single argument then there is no application
    // to launch
    if(cfiCount == 0)
    {
        wxLogDebug(wxT("wxMacLaunch No file to launch!"));
        return false ;
    }

    // Path to bundle
    wxString path = *argv++;

    // Create a CFURL for the application path
    // Created this way because we are opening a bundle which is a directory
    CFURLRef cfurlApp =
        CFURLCreateWithFileSystemPath(
            kCFAllocatorDefault,
            wxCFStringRef(path),
            kDefaultPathStyle,
            true); //false == not a directory

    // Check for error from the CFURL
    if(!cfurlApp)
    {
        wxLogDebug(wxT("wxMacLaunch Can't open path: %s"), path.c_str());
        return false ;
    }

    // Create a CFBundle from the CFURL created earlier
    CFBundleRef cfbApp = CFBundleCreate(kCFAllocatorDefault, cfurlApp);

    // Check to see if CFBundleCreate returned an error,
    // and if it did this was an invalid bundle or not a bundle
    // at all (maybe a simple directory etc.)
    if(!cfbApp)
    {
        wxLogDebug(wxT("wxMacLaunch Bad bundle: %s"), path.c_str());
        CFRelease(cfurlApp);
        return false ;
    }

    // Get the bundle type and make sure its an 'APPL' bundle
    // Otherwise we're dealing with something else here...
    UInt32 dwBundleType, dwBundleCreator;
    CFBundleGetPackageInfo(cfbApp, &dwBundleType, &dwBundleCreator);
    if(dwBundleType != 'APPL')
    {
        wxLogDebug(wxT("wxMacLaunch Not an APPL bundle: %s"), path.c_str());
        CFRelease(cfbApp);
        CFRelease(cfurlApp);
        return false ;
    }

    // Create a CFArray for dealing with the command line
    // arguments to the bundle
    CFMutableArrayRef cfaFiles = CFArrayCreateMutable(kCFAllocatorDefault,
                                    cfiCount-1, &kCFTypeArrayCallBacks);
    if(!cfaFiles) //This should never happen
    {
        wxLogDebug(wxT("wxMacLaunch Could not create CFMutableArray"));
        CFRelease(cfbApp);
        CFRelease(cfurlApp);
        return false ;
    }

    // Loop through command line arguments to the bundle,
    // turn them into CFURLs and then put them in cfaFiles
    // For use to launch services call
    for( ; *argv != NULL ; ++argv)
    {
        // Check for '<' as this will ring true for
        // CFURLCreateWithString but is generally not considered
        // typical on mac but is usually passed here from wxExecute
        if (wxStrcmp(*argv, wxT("<")) == 0)
            continue;


        CFURLRef cfurlCurrentFile;    // CFURL to hold file path
        wxFileName argfn(*argv);     // Filename for path

        if(argfn.DirExists())
        {
            // First, try creating as a directory
            cfurlCurrentFile = CFURLCreateWithFileSystemPath(
                                kCFAllocatorDefault,
                                wxCFStringRef(*argv),
                                kDefaultPathStyle,
                                true); //true == directory
        }
        else if(argfn.FileExists())
        {
            // And if it isn't a directory try creating it
            // as a regular file
            cfurlCurrentFile = CFURLCreateWithFileSystemPath(
                                kCFAllocatorDefault,
                                wxCFStringRef(*argv),
                                kDefaultPathStyle,
                                false); //false == regular file
        }
        else
        {
            // Argument did not refer to
            // an entry in the local filesystem,
            // so try creating it through CFURLCreateWithString
            cfurlCurrentFile = CFURLCreateWithString(
                                kCFAllocatorDefault,
                                wxCFStringRef(*argv),
                                NULL);
        }

        // Continue in the loop if the CFURL could not be created
        if(!cfurlCurrentFile)
        {
            wxLogDebug(
                wxT("wxMacLaunch Could not create CFURL for argument:%s"),
                *argv);
            continue;
        }

        // Add the valid CFURL to the argument array and then
        // release it as the CFArray adds a ref count to it
        CFArrayAppendValue(
            cfaFiles,
            cfurlCurrentFile
                        );
        CFRelease(cfurlCurrentFile); // array has retained it
    }

    // Create a LSLaunchURLSpec for use with LSOpenFromURLSpec
    // Note that there are several flag options (launchFlags) such
    // as kLSLaunchDontSwitch etc. and maybe we could be more
    // picky about the flags we choose
    LSLaunchURLSpec launchspec;
    launchspec.appURL = cfurlApp;
    launchspec.itemURLs = cfaFiles;
    launchspec.passThruParams = NULL; //AEDesc*
    launchspec.launchFlags = kLSLaunchDefaults;
    launchspec.asyncRefCon = NULL;

    // Finally, call LSOpenFromURL spec with our arguments
    // 2nd parameter is a pointer to a CFURL that gets
    // the actual path launched by the function
    OSStatus status = LSOpenFromURLSpec(&launchspec, NULL);

    // Cleanup corefoundation references
    CFRelease(cfbApp);
    CFRelease(cfurlApp);
    CFRelease(cfaFiles);

    // Check for error from LSOpenFromURLSpec
    if(status != noErr)
    {
        wxLogDebug(wxT("wxMacLaunch LSOpenFromURLSpec Error: %d"),
                   (int)status);
        return false ;
    }

    // No error from LSOpenFromURLSpec, so app was launched
    return true ;
}

#endif // wxOSX_USE_COCOA_OR_CARBON
