/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/utils_osx.cpp
// Purpose:     Various utilities
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#include "wx/wxprec.h"

#include "wx/utils.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #include "wx/log.h"
    #if wxUSE_GUI
        #include "wx/toplevel.h"
        #include "wx/font.h"
    #endif
#endif

#include "wx/apptrait.h"

#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// #include "MoreFilesX.h"

#include <AudioToolbox/AudioServices.h>

#if wxUSE_GUI
    #include "wx/private/launchbrowser.h"
#endif

#include "wx/osx/private.h"
#include "wx/osx/private/timer.h"

#include "wx/evtloop.h"

// Check whether this window wants to process messages, e.g. Stop button
// in long calculations.
bool wxCheckForInterrupt(wxWindow *WXUNUSED(wnd))
{
    // TODO
    return false;
}

// Return true if we have a colour display
bool wxColourDisplay()
{
    // always the case on OS X
    return true;
}


#if wxOSX_USE_COCOA_OR_CARBON

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= 1070) && (MAC_OS_X_VERSION_MIN_REQUIRED < 1060)
// bring back declaration so that we can support deployment targets < 10_6
CG_EXTERN size_t CGDisplayBitsPerPixel(CGDirectDisplayID display)
CG_AVAILABLE_BUT_DEPRECATED(__MAC_10_0, __MAC_10_6,
                            __IPHONE_NA, __IPHONE_NA);
#endif

// Returns depth of screen
int wxDisplayDepth()
{
    int theDepth = 0;
    
    CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(kCGDirectMainDisplay);
    CFStringRef encoding = CGDisplayModeCopyPixelEncoding(currentMode);

    if(CFStringCompare(encoding, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        theDepth = 32;
    else if(CFStringCompare(encoding, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        theDepth = 16;
    else if(CFStringCompare(encoding, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        theDepth = 8;
    else
        theDepth = 32; // some reasonable default

    CFRelease(encoding);
    CGDisplayModeRelease(currentMode);

    return theDepth;
}

// Get size of display
void wxDisplaySize(int *width, int *height)
{
    // TODO adapt for multi-displays
    CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    if ( width )
        *width = (int)bounds.size.width ;
    if ( height )
        *height = (int)bounds.size.height;
}

#if wxUSE_GUI

// ----------------------------------------------------------------------------
// Launch document with default app
// ----------------------------------------------------------------------------

bool wxLaunchDefaultApplication(const wxString& document, int flags)
{
    wxUnusedVar(flags);

    wxCFRef<CFMutableStringRef> cfMutableString(CFStringCreateMutableCopy(NULL, 0, wxCFStringRef(document)));
    CFStringNormalize(cfMutableString,kCFStringNormalizationFormD);
    wxCFRef<CFURLRef> curl(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfMutableString , kCFURLPOSIXPathStyle, false));
    OSStatus err = LSOpenCFURLRef( curl , NULL );

    if (err == noErr)
    {
        return true;
    }
    else
    {
        wxLogDebug(wxT("Default Application Launch error %d"), (int) err);
        return false;
    }
}

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

bool wxDoLaunchDefaultBrowser(const wxLaunchBrowserParams& params)
{
    wxCFRef< CFURLRef > curl( CFURLCreateWithString( kCFAllocatorDefault,
                              wxCFStringRef( params.url ), NULL ) );
    OSStatus err = LSOpenCFURLRef( curl , NULL );

    if (err == noErr)
    {
        return true;
    }
    else
    {
        wxLogDebug(wxT("Browser Launch error %d"), (int) err);
        return false;
    }
}

#endif // wxUSE_GUI

#endif

void wxDisplaySizeMM(int *width, int *height)
{
    wxDisplaySize(width, height);
    // on mac 72 is fixed (at least now;-)
    double cvPt2Mm = 25.4 / 72;

    if (width != NULL)
        *width = int( *width * cvPt2Mm );

    if (height != NULL)
        *height = int( *height * cvPt2Mm );
}


wxPortId wxGUIAppTraits::GetToolkitVersion(int *verMaj,
                                           int *verMin,
                                           int *verMicro) const
{
    // We suppose that toolkit version is the same as OS version under Mac
    wxGetOsVersion(verMaj, verMin, verMicro);

    return wxPORT_OSX;
}

wxEventLoopBase* wxGUIAppTraits::CreateEventLoop()
{
    return new wxEventLoop;
}

wxWindow* wxFindWindowAtPoint(wxWindow* win, const wxPoint& pt);

wxWindow* wxFindWindowAtPoint(const wxPoint& pt)
{
    return wxGenericFindWindowAtPoint( pt );
}

/*
    Return the generic RGB color space. This is a 'get' function and the caller should
    not release the returned value unless the caller retains it first. Usually callers
    of this routine will immediately use the returned colorspace with CoreGraphics
    so they typically do not need to retain it themselves.

    This function creates the generic RGB color space once and hangs onto it so it can
    return it whenever this function is called.
*/

CGColorSpaceRef wxMacGetGenericRGBColorSpace()
{
    static wxCFRef<CGColorSpaceRef> genericRGBColorSpace;

    if (genericRGBColorSpace == NULL)
    {
#if wxOSX_USE_IPHONE
        genericRGBColorSpace.reset( CGColorSpaceCreateDeviceRGB() );
#else
        genericRGBColorSpace.reset( CGColorSpaceCreateWithName( kCGColorSpaceGenericRGB ) );
#endif
    }

    return genericRGBColorSpace;
}

#if wxOSX_USE_COCOA_OR_CARBON

CGColorRef wxMacCreateCGColorFromHITheme( ThemeBrush brush )
{
    const int maxcachedbrush = 58+5; // negative indices are for metabrushes, cache down to -5)
    int brushindex = brush+5;
    if ( brushindex < 0 || brushindex > maxcachedbrush )
    {
        CGColorRef color ;
        HIThemeBrushCreateCGColor( brush, &color );
        return color;
    }
    else
    {
        static bool inited = false;
        static CGColorRef themecolors[maxcachedbrush+1];
        if ( !inited )
        {
            for ( int i = 0 ; i <= maxcachedbrush ; ++i )
                HIThemeBrushCreateCGColor( i-5, &themecolors[i] );
            inited = true;
        }
        return CGColorRetain(themecolors[brushindex ]);
    }
}

//---------------------------------------------------------------------------
// Mac Specific string utility functions
//---------------------------------------------------------------------------

void wxMacStringToPascal( const wxString&from , unsigned char * to )
{
    wxCharBuffer buf = from.mb_str( wxConvLocal );
    size_t len = buf.length();

    if ( len > 255 )
        len = 255;
    to[0] = len;
    memcpy( (char*) &to[1] , buf , len );
}

wxString wxMacMakeStringFromPascal( const unsigned char * from )
{
    return wxString( (char*) &from[1] , wxConvLocal , from[0] );
}

#endif // wxOSX_USE_COCOA_OR_CARBON
