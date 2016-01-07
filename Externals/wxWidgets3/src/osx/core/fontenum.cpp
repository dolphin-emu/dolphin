///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/fontenum.cpp
// Purpose:     wxFontEnumerator class for MacOS
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FONTENUM

#include "wx/fontenum.h"

#ifndef WX_PRECOMP
    #include "wx/font.h"
    #include "wx/intl.h"
#endif

#include "wx/fontutil.h"
#include "wx/fontmap.h"
#include "wx/encinfo.h"

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// wxFontEnumerator
// ----------------------------------------------------------------------------

#if wxOSX_USE_IPHONE
extern CFArrayRef CopyAvailableFontFamilyNames();
#endif

bool wxFontEnumerator::EnumerateFacenames(wxFontEncoding encoding,
                                          bool fixedWidthOnly)
{
     wxArrayString fontFamilies ;

    wxUint32 macEncoding = wxMacGetSystemEncFromFontEnc(encoding) ;

    {
        CFArrayRef cfFontFamilies = nil;

#if wxOSX_USE_COCOA_OR_CARBON
        cfFontFamilies = CTFontManagerCopyAvailableFontFamilyNames();
#elif wxOSX_USE_IPHONE
        cfFontFamilies = CopyAvailableFontFamilyNames();
#endif
        
        CFIndex count = CFArrayGetCount(cfFontFamilies);
        for(CFIndex i = 0; i < count; i++)
        {
            CFStringRef fontName = (CFStringRef)CFArrayGetValueAtIndex(cfFontFamilies, i);

            if ( encoding != wxFONTENCODING_SYSTEM || fixedWidthOnly)
            {
                wxCFRef<CTFontRef> font(CTFontCreateWithName(fontName, 12.0, NULL));
                if ( encoding != wxFONTENCODING_SYSTEM )
                {
                    CFStringEncoding fontFamiliyEncoding = CTFontGetStringEncoding(font);
                    if ( fontFamiliyEncoding != macEncoding )
                        continue;
                }
                
                if ( fixedWidthOnly )
                {
                    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(font);
                    if ( (traits & kCTFontMonoSpaceTrait) == 0 )
                        continue;
                }
                
            }
            
            wxCFStringRef cfName(wxCFRetain(fontName)) ;
            fontFamilies.Add(cfName.AsString(wxLocale::GetSystemEncoding()));
        }
        
        CFRelease(cfFontFamilies);
    }
    for ( size_t i = 0 ; i < fontFamilies.Count() ; ++i )
    {
        if ( OnFacename( fontFamilies[i] ) == false )
            break ;
    }

    return true;
}

bool wxFontEnumerator::EnumerateEncodings(const wxString& WXUNUSED(family))
{
    wxFAIL_MSG(wxT("wxFontEnumerator::EnumerateEncodings() not yet implemented"));

    return true;
}

#endif // wxUSE_FONTENUM
