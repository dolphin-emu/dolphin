/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/cfstring.cpp
// Purpose:     wxCFStringHolder and other string functions
// Author:      Stefan Csomor
// Modified by:
// Created:     2004-10-29 (from code in src/osx/carbon/utils.cpp)
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
// Usage:       Darwin (base library)
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/intl.h"
    #if wxUSE_GUI
        #include "wx/font.h"
    #endif
#endif

#include "wx/osx/core/cfstring.h"

#include <CoreFoundation/CoreFoundation.h>


void wxMacConvertNewlines13To10( char * data )
{
    char * buf = data ;
    while( (buf=strchr(buf,0x0d)) != NULL )
    {
        *buf = 0x0a ;
        buf++ ;
    }
}

void wxMacConvertNewlines10To13( char * data )
{
    char * buf = data ;
    while( (buf=strchr(buf,0x0a)) != NULL )
    {
        *buf = 0x0d ;
        buf++ ;
    }
}

const wxString sCR((wxChar)13);
const wxString sLF((wxChar)10);

void wxMacConvertNewlines13To10( wxString * data )
{
    data->Replace( sCR,sLF);
}

void wxMacConvertNewlines10To13( wxString * data )
{
    data->Replace( sLF,sCR);
}

wxUint32 wxMacGetSystemEncFromFontEnc(wxFontEncoding encoding)
{
    CFStringEncoding enc = 0 ;
    if ( encoding == wxFONTENCODING_DEFAULT )
    {
#if wxUSE_GUI
        encoding = wxFont::GetDefaultEncoding() ;
#else
        encoding = wxFONTENCODING_SYSTEM; // to be set below
#endif
    }

    if ( encoding == wxFONTENCODING_SYSTEM )
    {
        enc = CFStringGetSystemEncoding();
    }

    switch( encoding)
    {
    case wxFONTENCODING_ISO8859_1 :
        enc = kCFStringEncodingISOLatin1 ;
        break ;
    case wxFONTENCODING_ISO8859_2 :
        enc = kCFStringEncodingISOLatin2;
        break ;
    case wxFONTENCODING_ISO8859_3 :
        enc = kCFStringEncodingISOLatin3 ;
        break ;
    case wxFONTENCODING_ISO8859_4 :
        enc = kCFStringEncodingISOLatin4;
        break ;
    case wxFONTENCODING_ISO8859_5 :
        enc = kCFStringEncodingISOLatinCyrillic;
        break ;
    case wxFONTENCODING_ISO8859_6 :
        enc = kCFStringEncodingISOLatinArabic;
        break ;
    case wxFONTENCODING_ISO8859_7 :
        enc = kCFStringEncodingISOLatinGreek;
        break ;
    case wxFONTENCODING_ISO8859_8 :
        enc = kCFStringEncodingISOLatinHebrew;
        break ;
    case wxFONTENCODING_ISO8859_9 :
        enc = kCFStringEncodingISOLatin5;
        break ;
    case wxFONTENCODING_ISO8859_10 :
        enc = kCFStringEncodingISOLatin6;
        break ;
    case wxFONTENCODING_ISO8859_13 :
        enc = kCFStringEncodingISOLatin7;
        break ;
    case wxFONTENCODING_ISO8859_14 :
        enc = kCFStringEncodingISOLatin8;
        break ;
    case wxFONTENCODING_ISO8859_15 :
        enc = kCFStringEncodingISOLatin9;
        break ;

    case wxFONTENCODING_KOI8 :
        enc = kCFStringEncodingKOI8_R;
        break ;
    case wxFONTENCODING_ALTERNATIVE : // MS-DOS CP866
        enc = kCFStringEncodingDOSRussian;
        break ;
/*
    case wxFONTENCODING_BULGARIAN :
        enc = ;
        break ;
*/
    case wxFONTENCODING_CP437 :
        enc =kCFStringEncodingDOSLatinUS ;
        break ;
    case wxFONTENCODING_CP850 :
        enc = kCFStringEncodingDOSLatin1;
        break ;
    case wxFONTENCODING_CP852 :
        enc = kCFStringEncodingDOSLatin2;
        break ;
    case wxFONTENCODING_CP855 :
        enc = kCFStringEncodingDOSCyrillic;
        break ;
    case wxFONTENCODING_CP866 :
        enc =kCFStringEncodingDOSRussian ;
        break ;
    case wxFONTENCODING_CP874 :
        enc = kCFStringEncodingDOSThai;
        break ;
    case wxFONTENCODING_CP932 :
        enc = kCFStringEncodingDOSJapanese;
        break ;
    case wxFONTENCODING_CP936 :
        enc = kCFStringEncodingDOSChineseSimplif ;
        break ;
    case wxFONTENCODING_CP949 :
        enc = kCFStringEncodingDOSKorean;
        break ;
    case wxFONTENCODING_CP950 :
        enc = kCFStringEncodingDOSChineseTrad;
        break ;

    case wxFONTENCODING_CP1250 :
        enc = kCFStringEncodingWindowsLatin2;
        break ;
    case wxFONTENCODING_CP1251 :
        enc =kCFStringEncodingWindowsCyrillic ;
        break ;
    case wxFONTENCODING_CP1252 :
        enc =kCFStringEncodingWindowsLatin1 ;
        break ;
    case wxFONTENCODING_CP1253 :
        enc = kCFStringEncodingWindowsGreek;
        break ;
    case wxFONTENCODING_CP1254 :
        enc = kCFStringEncodingWindowsLatin5;
        break ;
    case wxFONTENCODING_CP1255 :
        enc =kCFStringEncodingWindowsHebrew ;
        break ;
    case wxFONTENCODING_CP1256 :
        enc =kCFStringEncodingWindowsArabic ;
        break ;
    case wxFONTENCODING_CP1257 :
        enc = kCFStringEncodingWindowsBalticRim;
        break ;
#if 0
    case wxFONTENCODING_UTF7 :
        enc = CreateTextEncoding(kCFStringEncodingUnicodeDefault,0,kUnicodeUTF7Format) ;
        break ;
#endif
    case wxFONTENCODING_UTF8 :
        enc = kCFStringEncodingUTF8;
        break ;
    case wxFONTENCODING_EUC_JP :
        enc = kCFStringEncodingEUC_JP;
        break ;
    case wxFONTENCODING_UTF16BE :
        enc = kCFStringEncodingUTF16BE;
        break ;
    case wxFONTENCODING_UTF16LE :
        enc = kCFStringEncodingUTF16LE;
        break ;
    case wxFONTENCODING_UTF32BE :
        enc = kCFStringEncodingUTF32BE;
        break ;
    case wxFONTENCODING_UTF32LE :
        enc = kCFStringEncodingUTF32LE;
        break ;

    case wxFONTENCODING_MACROMAN :
        enc = kCFStringEncodingMacRoman ;
        break ;
    case wxFONTENCODING_MACJAPANESE :
        enc = kCFStringEncodingMacJapanese ;
        break ;
    case wxFONTENCODING_MACCHINESETRAD :
        enc = kCFStringEncodingMacChineseTrad ;
        break ;
    case wxFONTENCODING_MACKOREAN :
        enc = kCFStringEncodingMacKorean ;
        break ;
    case wxFONTENCODING_MACARABIC :
        enc = kCFStringEncodingMacArabic ;
        break ;
    case wxFONTENCODING_MACHEBREW :
        enc = kCFStringEncodingMacHebrew ;
        break ;
    case wxFONTENCODING_MACGREEK :
        enc = kCFStringEncodingMacGreek ;
        break ;
    case wxFONTENCODING_MACCYRILLIC :
        enc = kCFStringEncodingMacCyrillic ;
        break ;
    case wxFONTENCODING_MACDEVANAGARI :
        enc = kCFStringEncodingMacDevanagari ;
        break ;
    case wxFONTENCODING_MACGURMUKHI :
        enc = kCFStringEncodingMacGurmukhi ;
        break ;
    case wxFONTENCODING_MACGUJARATI :
        enc = kCFStringEncodingMacGujarati ;
        break ;
    case wxFONTENCODING_MACORIYA :
        enc = kCFStringEncodingMacOriya ;
        break ;
    case wxFONTENCODING_MACBENGALI :
        enc = kCFStringEncodingMacBengali ;
        break ;
    case wxFONTENCODING_MACTAMIL :
        enc = kCFStringEncodingMacTamil ;
        break ;
    case wxFONTENCODING_MACTELUGU :
        enc = kCFStringEncodingMacTelugu ;
        break ;
    case wxFONTENCODING_MACKANNADA :
        enc = kCFStringEncodingMacKannada ;
        break ;
    case wxFONTENCODING_MACMALAJALAM :
        enc = kCFStringEncodingMacMalayalam ;
        break ;
    case wxFONTENCODING_MACSINHALESE :
        enc = kCFStringEncodingMacSinhalese ;
        break ;
    case wxFONTENCODING_MACBURMESE :
        enc = kCFStringEncodingMacBurmese ;
        break ;
    case wxFONTENCODING_MACKHMER :
        enc = kCFStringEncodingMacKhmer ;
        break ;
    case wxFONTENCODING_MACTHAI :
        enc = kCFStringEncodingMacThai ;
        break ;
    case wxFONTENCODING_MACLAOTIAN :
        enc = kCFStringEncodingMacLaotian ;
        break ;
    case wxFONTENCODING_MACGEORGIAN :
        enc = kCFStringEncodingMacGeorgian ;
        break ;
    case wxFONTENCODING_MACARMENIAN :
        enc = kCFStringEncodingMacArmenian ;
        break ;
    case wxFONTENCODING_MACCHINESESIMP :
        enc = kCFStringEncodingMacChineseSimp ;
        break ;
    case wxFONTENCODING_MACTIBETAN :
        enc = kCFStringEncodingMacTibetan ;
        break ;
    case wxFONTENCODING_MACMONGOLIAN :
        enc = kCFStringEncodingMacMongolian ;
        break ;
    case wxFONTENCODING_MACETHIOPIC :
        enc = kCFStringEncodingMacEthiopic ;
        break ;
    case wxFONTENCODING_MACCENTRALEUR :
        enc = kCFStringEncodingMacCentralEurRoman ;
        break ;
    case wxFONTENCODING_MACVIATNAMESE :
        enc = kCFStringEncodingMacVietnamese ;
        break ;
    case wxFONTENCODING_MACARABICEXT :
        enc = kCFStringEncodingMacExtArabic ;
        break ;
    case wxFONTENCODING_MACSYMBOL :
        enc = kCFStringEncodingMacSymbol ;
        break ;
    case wxFONTENCODING_MACDINGBATS :
        enc = kCFStringEncodingMacDingbats ;
        break ;
    case wxFONTENCODING_MACTURKISH :
        enc = kCFStringEncodingMacTurkish ;
        break ;
    case wxFONTENCODING_MACCROATIAN :
        enc = kCFStringEncodingMacCroatian ;
        break ;
    case wxFONTENCODING_MACICELANDIC :
        enc = kCFStringEncodingMacIcelandic ;
        break ;
    case wxFONTENCODING_MACROMANIAN :
        enc = kCFStringEncodingMacRomanian ;
        break ;
    case wxFONTENCODING_MACCELTIC :
        enc = kCFStringEncodingMacCeltic ;
        break ;
    case wxFONTENCODING_MACGAELIC :
        enc = kCFStringEncodingMacGaelic ;
        break ;
    case wxFONTENCODING_MACKEYBOARD :
        enc = 41; /* kTextEncodingMacKeyboardGlyphs ; */
        break ;
    default : // to make gcc happy
        break ;
    };
    return enc ;
}

wxFontEncoding wxMacGetFontEncFromSystemEnc(wxUint32 encoding)
{
    wxFontEncoding enc = wxFONTENCODING_DEFAULT ;

    switch( encoding)
    {
    case kCFStringEncodingISOLatin1  :
        enc = wxFONTENCODING_ISO8859_1 ;
        break ;
    case kCFStringEncodingISOLatin2 :
        enc = wxFONTENCODING_ISO8859_2;
        break ;
    case kCFStringEncodingISOLatin3 :
        enc = wxFONTENCODING_ISO8859_3 ;
        break ;
    case kCFStringEncodingISOLatin4 :
        enc = wxFONTENCODING_ISO8859_4;
        break ;
    case kCFStringEncodingISOLatinCyrillic :
        enc = wxFONTENCODING_ISO8859_5;
        break ;
    case kCFStringEncodingISOLatinArabic :
        enc = wxFONTENCODING_ISO8859_6;
        break ;
    case kCFStringEncodingISOLatinGreek :
        enc = wxFONTENCODING_ISO8859_7;
        break ;
    case kCFStringEncodingISOLatinHebrew :
        enc = wxFONTENCODING_ISO8859_8;
        break ;
    case kCFStringEncodingISOLatin5 :
        enc = wxFONTENCODING_ISO8859_9;
        break ;
    case kCFStringEncodingISOLatin6 :
        enc = wxFONTENCODING_ISO8859_10;
        break ;
    case kCFStringEncodingISOLatin7 :
        enc = wxFONTENCODING_ISO8859_13;
        break ;
    case kCFStringEncodingISOLatin8 :
        enc = wxFONTENCODING_ISO8859_14;
        break ;
    case kCFStringEncodingISOLatin9 :
        enc =wxFONTENCODING_ISO8859_15 ;
        break ;

    case kCFStringEncodingKOI8_R :
        enc = wxFONTENCODING_KOI8;
        break ;
/*
    case  :
        enc = wxFONTENCODING_BULGARIAN;
        break ;
*/
    case kCFStringEncodingDOSLatinUS :
        enc = wxFONTENCODING_CP437;
        break ;
    case kCFStringEncodingDOSLatin1 :
        enc = wxFONTENCODING_CP850;
        break ;
    case kCFStringEncodingDOSLatin2 :
        enc =wxFONTENCODING_CP852 ;
        break ;
    case kCFStringEncodingDOSCyrillic :
        enc = wxFONTENCODING_CP855;
        break ;
    case kCFStringEncodingDOSRussian :
        enc = wxFONTENCODING_CP866;
        break ;
    case kCFStringEncodingDOSThai :
        enc =wxFONTENCODING_CP874 ;
        break ;
    case kCFStringEncodingDOSJapanese :
        enc = wxFONTENCODING_CP932;
        break ;
    case kCFStringEncodingDOSChineseSimplif :
        enc = wxFONTENCODING_CP936;
        break ;
    case kCFStringEncodingDOSKorean :
        enc = wxFONTENCODING_CP949;
        break ;
    case kCFStringEncodingDOSChineseTrad :
        enc = wxFONTENCODING_CP950;
        break ;

    case kCFStringEncodingWindowsLatin2 :
        enc = wxFONTENCODING_CP1250;
        break ;
    case kCFStringEncodingWindowsCyrillic :
        enc = wxFONTENCODING_CP1251;
        break ;
    case kCFStringEncodingWindowsLatin1 :
        enc = wxFONTENCODING_CP1252;
        break ;
    case kCFStringEncodingWindowsGreek :
        enc = wxFONTENCODING_CP1253;
        break ;
    case kCFStringEncodingWindowsLatin5 :
        enc = wxFONTENCODING_CP1254;
        break ;
    case kCFStringEncodingWindowsHebrew :
        enc = wxFONTENCODING_CP1255;
        break ;
    case kCFStringEncodingWindowsArabic :
        enc = wxFONTENCODING_CP1256;
        break ;
    case kCFStringEncodingWindowsBalticRim :
        enc =wxFONTENCODING_CP1257 ;
        break ;
    case kCFStringEncodingEUC_JP :
        enc = wxFONTENCODING_EUC_JP;
        break ;

    case kCFStringEncodingUTF8 :
        enc = wxFONTENCODING_UTF8;
        break ;
    case kCFStringEncodingUTF16BE :
        enc = wxFONTENCODING_UTF16BE;
        break ;
    case kCFStringEncodingUTF16LE :
        enc = wxFONTENCODING_UTF16LE;
        break ;
    case kCFStringEncodingUTF32BE :
        enc = wxFONTENCODING_UTF32BE;
        break ;
    case kCFStringEncodingUTF32LE :
        enc = wxFONTENCODING_UTF32LE;
        break ;

#if 0
    case wxFONTENCODING_UTF7 :
        enc = CreateTextEncoding(kCFStringEncodingUnicodeDefault,0,kUnicodeUTF7Format) ;
        break ;
#endif
    case kCFStringEncodingMacRoman :
        enc = wxFONTENCODING_MACROMAN ;
        break ;
    case kCFStringEncodingMacJapanese :
        enc = wxFONTENCODING_MACJAPANESE ;
        break ;
    case kCFStringEncodingMacChineseTrad :
        enc = wxFONTENCODING_MACCHINESETRAD ;
        break ;
    case kCFStringEncodingMacKorean :
        enc = wxFONTENCODING_MACKOREAN ;
        break ;
    case kCFStringEncodingMacArabic :
        enc =wxFONTENCODING_MACARABIC ;
        break ;
    case kCFStringEncodingMacHebrew :
        enc = wxFONTENCODING_MACHEBREW ;
        break ;
    case kCFStringEncodingMacGreek :
        enc = wxFONTENCODING_MACGREEK ;
        break ;
    case kCFStringEncodingMacCyrillic :
        enc = wxFONTENCODING_MACCYRILLIC ;
        break ;
    case kCFStringEncodingMacDevanagari :
        enc = wxFONTENCODING_MACDEVANAGARI ;
        break ;
    case kCFStringEncodingMacGurmukhi :
        enc = wxFONTENCODING_MACGURMUKHI ;
        break ;
    case kCFStringEncodingMacGujarati :
        enc = wxFONTENCODING_MACGUJARATI ;
        break ;
    case kCFStringEncodingMacOriya :
        enc =wxFONTENCODING_MACORIYA ;
        break ;
    case kCFStringEncodingMacBengali :
        enc =wxFONTENCODING_MACBENGALI ;
        break ;
    case kCFStringEncodingMacTamil :
        enc = wxFONTENCODING_MACTAMIL ;
        break ;
    case kCFStringEncodingMacTelugu :
        enc = wxFONTENCODING_MACTELUGU ;
        break ;
    case kCFStringEncodingMacKannada :
        enc = wxFONTENCODING_MACKANNADA ;
        break ;
    case kCFStringEncodingMacMalayalam :
        enc = wxFONTENCODING_MACMALAJALAM ;
        break ;
    case kCFStringEncodingMacSinhalese :
        enc = wxFONTENCODING_MACSINHALESE ;
        break ;
    case kCFStringEncodingMacBurmese :
        enc = wxFONTENCODING_MACBURMESE ;
        break ;
    case kCFStringEncodingMacKhmer :
        enc = wxFONTENCODING_MACKHMER ;
        break ;
    case kCFStringEncodingMacThai :
        enc = wxFONTENCODING_MACTHAI ;
        break ;
    case kCFStringEncodingMacLaotian :
        enc = wxFONTENCODING_MACLAOTIAN ;
        break ;
    case kCFStringEncodingMacGeorgian :
        enc = wxFONTENCODING_MACGEORGIAN ;
        break ;
    case kCFStringEncodingMacArmenian :
        enc = wxFONTENCODING_MACARMENIAN ;
        break ;
    case kCFStringEncodingMacChineseSimp :
        enc = wxFONTENCODING_MACCHINESESIMP ;
        break ;
    case kCFStringEncodingMacTibetan :
        enc = wxFONTENCODING_MACTIBETAN ;
        break ;
    case kCFStringEncodingMacMongolian :
        enc = wxFONTENCODING_MACMONGOLIAN ;
        break ;
    case kCFStringEncodingMacEthiopic :
        enc = wxFONTENCODING_MACETHIOPIC ;
        break ;
    case kCFStringEncodingMacCentralEurRoman:
        enc = wxFONTENCODING_MACCENTRALEUR  ;
        break ;
    case kCFStringEncodingMacVietnamese:
        enc = wxFONTENCODING_MACVIATNAMESE  ;
        break ;
    case kCFStringEncodingMacExtArabic :
        enc = wxFONTENCODING_MACARABICEXT ;
        break ;
    case kCFStringEncodingMacSymbol :
        enc = wxFONTENCODING_MACSYMBOL ;
        break ;
    case kCFStringEncodingMacDingbats :
        enc = wxFONTENCODING_MACDINGBATS ;
        break ;
    case kCFStringEncodingMacTurkish :
        enc = wxFONTENCODING_MACTURKISH ;
        break ;
    case kCFStringEncodingMacCroatian :
        enc = wxFONTENCODING_MACCROATIAN ;
        break ;
    case kCFStringEncodingMacIcelandic :
        enc = wxFONTENCODING_MACICELANDIC ;
        break ;
    case kCFStringEncodingMacRomanian :
        enc = wxFONTENCODING_MACROMANIAN ;
        break ;
    case kCFStringEncodingMacCeltic :
        enc = wxFONTENCODING_MACCELTIC ;
        break ;
    case kCFStringEncodingMacGaelic :
        enc = wxFONTENCODING_MACGAELIC ;
        break ;
    case 41 /* kTextEncodingMacKeyboardGlyphs */ :
        enc = wxFONTENCODING_MACKEYBOARD ;
        break ;
    } ;
    return enc ;
}


//
// CFStringRefs
//

// converts this string into a core foundation string with optional pc 2 mac encoding

wxCFStringRef::wxCFStringRef( const wxString &st , wxFontEncoding WXUNUSED_IN_UNICODE(encoding) )
{
    if (st.IsEmpty())
    {
        reset( wxCFRetain( CFSTR("") ) );
    }
    else
    {
        wxString str = st ;
        wxMacConvertNewlines13To10( &str ) ;
#if wxUSE_UNICODE
#if wxUSE_UNICODE_WCHAR
        // native = wchar_t 4 bytes for us
        const wchar_t * const data = str.wc_str();
        const size_t size = str.length()*sizeof(wchar_t);
        CFStringBuiltInEncodings cfencoding = kCFStringEncodingUTF32Native;
#elif wxUSE_UNICODE_UTF8
        // native = utf8
        const char * const data = str.utf8_str();
        const size_t size = str.utf8_length();
        CFStringBuiltInEncodings cfencoding = kCFStringEncodingUTF8;
#else
    #error "unsupported Unicode representation"
#endif

        reset( CFStringCreateWithBytes( kCFAllocatorDefault,
            (const UInt8*)data, size, cfencoding, false /* no BOM */ ) );
#else // not wxUSE_UNICODE
        reset( CFStringCreateWithCString( kCFAllocatorSystemDefault , str.c_str() ,
            wxMacGetSystemEncFromFontEnc( encoding ) ) );
#endif
    }
}

wxString wxCFStringRef::AsStringWithNormalizationFormC( CFStringRef ref, wxFontEncoding encoding )
{
    if ( !ref )
        return wxEmptyString ;

    CFMutableStringRef cfMutableString = CFStringCreateMutableCopy(NULL, 0, ref);
    CFStringNormalize(cfMutableString,kCFStringNormalizationFormC);
    wxString str = wxCFStringRef::AsString(cfMutableString,encoding);
    CFRelease(cfMutableString);
    return str;
}

wxString wxCFStringRef::AsString( CFStringRef ref, wxFontEncoding WXUNUSED_IN_UNICODE(encoding) )
{
    if ( !ref )
        return wxEmptyString ;

    Size cflen = CFStringGetLength( ref )  ;

    CFStringEncoding cfencoding;
    wxString result;
#if wxUSE_UNICODE
  #if wxUSE_UNICODE_WCHAR
    cfencoding = kCFStringEncodingUTF32Native;
  #elif wxUSE_UNICODE_UTF8
    cfencoding = kCFStringEncodingUTF8;
  #else
    #error "unsupported unicode representation"
  #endif
#else
    cfencoding = wxMacGetSystemEncFromFontEnc( encoding );
#endif

    CFIndex cStrLen ;
    CFStringGetBytes( ref , CFRangeMake(0, cflen) , cfencoding ,
        '?' , false , NULL , 0 , &cStrLen ) ;
    char* buf = new char[cStrLen];
    CFStringGetBytes( ref , CFRangeMake(0, cflen) , cfencoding,
        '?' , false , (unsigned char*) buf , cStrLen , &cStrLen) ;

#if wxUSE_UNICODE
  #if wxUSE_UNICODE_WCHAR
    result = wxString( (const wchar_t*) buf , cStrLen/4);
  #elif wxUSE_UNICODE_UTF8
    result = wxString::FromUTF8( buf, cStrLen );
  #else
    #error "unsupported unicode representation"
  #endif
#else
    result = wxString(buf, cStrLen) ;
#endif

    delete[] buf ;
    wxMacConvertNewlines10To13( &result);
    return result ;
}

wxString wxCFStringRef::AsString(wxFontEncoding encoding) const
{
    return AsString( get(), encoding );
}

#ifdef __WXMAC__

wxString wxCFStringRef::AsString( NSString* ref, wxFontEncoding encoding )
{
    return AsString( (CFStringRef) ref, encoding );
}

wxString wxCFStringRef::AsStringWithNormalizationFormC( NSString* ref, wxFontEncoding encoding )
{
    return AsStringWithNormalizationFormC( (CFStringRef) ref, encoding );
}

#endif

//
// wxMacUniCharBuffer
//

wxMacUniCharBuffer::wxMacUniCharBuffer( const wxString &str )
{
    wxMBConvUTF16 converter ;
#if wxUSE_UNICODE
    size_t unicharlen = converter.WC2MB( NULL , str.wc_str() , 0 ) ;
    m_ubuf = (UniChar*) malloc( unicharlen + 2 ) ;
    converter.WC2MB( (char*) m_ubuf , str.wc_str(), unicharlen + 2 ) ;
#else
    const wxWCharBuffer wchar = str.wc_str( wxConvLocal ) ;
    size_t unicharlen = converter.WC2MB( NULL , wchar.data() , 0 ) ;
    m_ubuf = (UniChar*) malloc( unicharlen + 2 ) ;
    converter.WC2MB( (char*) m_ubuf , wchar.data() , unicharlen + 2 ) ;
#endif
    m_chars = unicharlen / 2 ;
}

wxMacUniCharBuffer::~wxMacUniCharBuffer()
{
    free( m_ubuf ) ;
}

UniCharPtr wxMacUniCharBuffer::GetBuffer()
{
    return m_ubuf ;
}

UniCharCount wxMacUniCharBuffer::GetChars()
{
    return m_chars ;
}
