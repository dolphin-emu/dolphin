/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/strconv_cf.cpp
// Purpose:     Unicode conversion classes
// Author:      David Elliott
// Modified by:
// Created:     2007-07-06
// Copyright:   (c) 2007 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
#endif

#include "wx/strconv.h"
#include "wx/fontmap.h"

#ifdef __DARWIN__

#include "wx/osx/core/private/strconv_cf.h"
#include "wx/osx/core/cfref.h"


// ============================================================================
// CoreFoundation conversion classes
// ============================================================================

/* Provide factory functions for unit tests.  Not in any header.  Do not
 * assume ABI compatibility even within a given wxWidgets release.
 */

#if wxUSE_FONTMAP
WXDLLIMPEXP_BASE wxMBConv* new_wxMBConv_cf( const char* name)
{
    wxMBConv_cf *result = new wxMBConv_cf(name);
    if(!result->IsOk())
    {
        delete result;
        return NULL;
    }
    else
        return result;
}
#endif // wxUSE_FONTMAP

WXDLLIMPEXP_BASE wxMBConv* new_wxMBConv_cf(wxFontEncoding encoding)
{
    wxMBConv_cf *result = new wxMBConv_cf(encoding);
    if(!result->IsOk())
    {
        delete result;
        return NULL;
    }
    else
        return result;
}

// Provide a constant for the wchat_t encoding used by the host platform.
#ifdef WORDS_BIGENDIAN
    static const CFStringEncoding wxCFStringEncodingWcharT = kCFStringEncodingUTF32BE;
#else
    static const CFStringEncoding wxCFStringEncodingWcharT = kCFStringEncodingUTF32LE;
#endif

    size_t wxMBConv_cf::ToWChar(wchar_t * dst, size_t dstSize, const char * src, size_t srcSize) const
    {
        wxCHECK(src, wxCONV_FAILED);

        /* NOTE: This is wrong if the source encoding has an element size
         * other than char (e.g. it's kCFStringEncodingUnicode)
         * If the user specifies it, it's presumably right though.
         * Right now we don't support UTF-16 in anyway since wx can do a better job.
         */
        if(srcSize == wxNO_LEN)
            srcSize = strlen(src) + 1;

        // First create the temporary CFString
        wxCFRef<CFStringRef> theString( CFStringCreateWithBytes (
                                                NULL, //the allocator
                                                (const UInt8*)src,
                                                srcSize,
                                                m_encoding,
                                                false //no BOM/external representation
                                                ));

        if ( theString == NULL )
            return wxCONV_FAILED;

        // Ensure that the string is in canonical composed form (NFC): this is
        // important because Darwin uses decomposed form (NFD) for e.g. file
        // names but we want to use NFC internally.
        wxCFRef<CFMutableStringRef>
            cfMutableString(CFStringCreateMutableCopy(NULL, 0, theString));
        CFStringNormalize(cfMutableString, kCFStringNormalizationFormC);
        theString = cfMutableString;

        /* NOTE: The string content includes the NULL element if the source string did
         * That means we have to do nothing special because the destination will have
         * the NULL element iff the source did and the NULL element will be included
         * in the count iff it was included in the source count.
         */


/* If we're compiling against Tiger headers we can support direct conversion
 * to UTF32.  If we are then run against a pre-Tiger system, the encoding
 * won't be available so we'll defer to the string->UTF-16->UTF-32 conversion.
 */
        if(CFStringIsEncodingAvailable(wxCFStringEncodingWcharT))
        {
            CFRange fullStringRange = CFRangeMake(0, CFStringGetLength(theString));
            CFIndex usedBufLen;

            CFIndex charsConverted = CFStringGetBytes(
                    theString,
                    fullStringRange,
                    wxCFStringEncodingWcharT,
                    0,
                    false,
                    // if dstSize is 0 then pass NULL to get required length in usedBufLen
                    dstSize != 0?(UInt8*)dst:NULL,
                    dstSize * sizeof(wchar_t),
                    &usedBufLen);

            if(charsConverted < CFStringGetLength(theString))
                return wxCONV_FAILED;

            /* usedBufLen is the number of bytes written, so we divide by
             * sizeof(wchar_t) to get the number of elements written.
             */
            wxASSERT( (usedBufLen % sizeof(wchar_t)) == 0 );

            // CFStringGetBytes does exactly the right thing when buffer
            // pointer is NULL and returns the number of bytes required
            return usedBufLen / sizeof(wchar_t);
        }
        else
        {
            // NOTE: Includes NULL iff source did
            /* NOTE: This is an approximation.  The eventual UTF-32 will
             * possibly have less elements but certainly not more.
             */
            size_t returnSize = CFStringGetLength(theString);

            if (dstSize == 0 || dst == NULL)
            {
                return returnSize;
            }

            // Convert the entire string.. too hard to figure out how many UTF-16 we'd need
            // for an undersized UTF-32 destination buffer.
            CFRange fullStringRange = CFRangeMake(0, CFStringGetLength(theString));
            UniChar *szUniCharBuffer = new UniChar[fullStringRange.length];

            CFStringGetCharacters(theString, fullStringRange, szUniCharBuffer);

            wxMBConvUTF16 converter;
            returnSize = converter.ToWChar( dst, dstSize, (const char*)szUniCharBuffer, fullStringRange.length );
            delete [] szUniCharBuffer;

            return returnSize;
        }
        // NOTREACHED
    }

    size_t wxMBConv_cf::FromWChar(char *dst, size_t dstSize, const wchar_t *src, size_t srcSize) const
    {
        wxCHECK(src, wxCONV_FAILED);

        if(srcSize == wxNO_LEN)
            srcSize = wxStrlen(src) + 1;

        // Temporary CFString
        wxCFRef<CFStringRef> theString;

/* If we're compiling against Tiger headers we can support direct conversion
 * from UTF32.  If we are then run against a pre-Tiger system, the encoding
 * won't be available so we'll defer to the UTF-32->UTF-16->string conversion.
 */
        if(CFStringIsEncodingAvailable(wxCFStringEncodingWcharT))
        {
            theString = wxCFRef<CFStringRef>(CFStringCreateWithBytes(
                    kCFAllocatorDefault,
                    (UInt8*)src,
                    srcSize * sizeof(wchar_t),
                    wxCFStringEncodingWcharT,
                    false));
        }
        else
        {
            wxMBConvUTF16 converter;
            size_t cbUniBuffer = converter.FromWChar( NULL, 0, src, srcSize );
            wxASSERT(cbUniBuffer % sizeof(UniChar));

            // Will be free'd by kCFAllocatorMalloc when CFString is released
            UniChar *tmpUniBuffer = (UniChar*)malloc(cbUniBuffer);

            cbUniBuffer = converter.FromWChar( (char*) tmpUniBuffer, cbUniBuffer, src, srcSize );
            wxASSERT(cbUniBuffer % sizeof(UniChar));

            theString = wxCFRef<CFStringRef>(CFStringCreateWithCharactersNoCopy(
                        kCFAllocatorDefault,
                        tmpUniBuffer,
                        cbUniBuffer / sizeof(UniChar),
                        kCFAllocatorMalloc
                    ));

        }

        wxCHECK(theString != NULL, wxCONV_FAILED);

        CFIndex usedBufLen;

        CFIndex charsConverted = CFStringGetBytes(
                theString,
                CFRangeMake(0, CFStringGetLength(theString)),
                m_encoding,
                0, // FAIL on unconvertible characters
                false, // not an external representation
                (UInt8*)dst,
                dstSize,
                &usedBufLen
            );

        // when dst is non-NULL, we check usedBufLen against dstSize as
        // CFStringGetBytes sometimes treats dst as being NULL when dstSize==0
        if( (charsConverted < CFStringGetLength(theString)) ||
                (dst && (size_t) usedBufLen > dstSize) )
            return wxCONV_FAILED;

        return usedBufLen;
    }

#endif // __DARWIN__


