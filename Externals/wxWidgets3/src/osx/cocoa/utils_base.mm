/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/utils_base.mm
// Purpose:     various OS X utility functions in the base lib
//              (extracted from cocoa/utils.mm)
// Author:      Tobias Taschner
// Created:     2016-02-10
// Copyright:   (c) wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/utils.h"
#include "wx/platinfo.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
#endif

#include "wx/apptrait.h"

#include "wx/osx/private.h"

#if (defined(__WXOSX_COCOA__) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_10) \
    || (defined(__WXOSX_IPHONE__) && defined(__IPHONE_8_0))
    #define wxHAS_NSPROCESSINFO 1
#endif

// our OS version is the same in non GUI and GUI cases
wxOperatingSystemId wxGetOsVersion(int *verMaj, int *verMin, int *verMicro)
{
#ifdef wxHAS_NSPROCESSINFO
    if ([NSProcessInfo instancesRespondToSelector:@selector(operatingSystemVersion)])
    {
        NSOperatingSystemVersion osVer = [NSProcessInfo processInfo].operatingSystemVersion;

        if ( verMaj != NULL )
            *verMaj = osVer.majorVersion;

        if ( verMin != NULL )
            *verMin = osVer.minorVersion;

        if ( verMicro != NULL )
            *verMicro = osVer.patchVersion;
    }
    else
#endif
    {
        // On OS X versions prior to 10.10 NSProcessInfo does not provide the OS version
        // Deprecated Gestalt calls are required instead
wxGCC_WARNING_SUPPRESS(deprecated-declarations)
        SInt32 maj, min, micro;
#ifdef __WXOSX_IPHONE__
        maj = 7;
        min = 0;
        micro = 0;
#else
        Gestalt(gestaltSystemVersionMajor, &maj);
        Gestalt(gestaltSystemVersionMinor, &min);
        Gestalt(gestaltSystemVersionBugFix, &micro);
#endif
wxGCC_WARNING_RESTORE()

        if ( verMaj != NULL )
            *verMaj = maj;

        if ( verMin != NULL )
            *verMin = min;

        if ( verMicro != NULL )
            *verMicro = micro;
    }

    return wxOS_MAC_OSX_DARWIN;
}

bool wxCheckOsVersion(int majorVsn, int minorVsn, int microVsn)
{
#ifdef wxHAS_NSPROCESSINFO
    if ([NSProcessInfo instancesRespondToSelector:@selector(isOperatingSystemAtLeastVersion:)])
    {
        NSOperatingSystemVersion osVer;
        osVer.majorVersion = majorVsn;
        osVer.minorVersion = minorVsn;
        osVer.patchVersion = microVsn;

        return [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:osVer] != NO;
    }
    else
#endif
    {
        int majorCur, minorCur, microCur;
        wxGetOsVersion(&majorCur, &minorCur, &microCur);

        return majorCur > majorVsn
            || (majorCur == majorVsn && minorCur >= minorVsn)
            || (majorCur == majorVsn && minorCur == minorVsn && microCur >= microVsn);
    }
}

wxString wxGetOsDescription()
{

    int majorVer, minorVer;
    wxGetOsVersion(&majorVer, &minorVer);

#ifndef __WXOSX_IPHONE__
    // Notice that neither the OS name itself nor the code names seem to be
    // ever translated, OS X itself uses the English words even for the
    // languages not using Roman alphabet.
    wxString osBrand = "OS X";
    wxString osName;
    if (majorVer == 10)
    {
        switch (minorVer)
        {
            case 7:
                osName = "Lion";
                // 10.7 was the last version where the "Mac" prefix was used
                osBrand = "Mac OS X";
                break;
            case 8:
                osName = "Mountain Lion";
                break;
            case 9:
                osName = "Mavericks";
                break;
            case 10:
                osName = "Yosemite";
                break;
            case 11:
                osName = "El Capitan";
                break;
        };
    }
#else
    wxString osBrand = "iOS";
    wxString osName;
#endif

    wxString osDesc = osBrand;
    if (!osName.empty())
        osDesc += " " + osName;

    NSString* osVersionString = [NSProcessInfo processInfo].operatingSystemVersionString;
    if (osVersionString)
        osDesc += " " + wxCFStringRef::AsString((CFStringRef)osVersionString);

    return osDesc;
}
