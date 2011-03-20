///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/convauto.cpp
// Purpose:     implementation of wxConvAuto
// Author:      Vadim Zeitlin
// Created:     2006-04-04
// RCS-ID:      $Id: convauto.cpp 66657 2011-01-08 18:05:33Z PC $
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
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

#include "wx/convauto.h"

// we use latin1 by default as it seems the least bad choice: the files we need
// to detect input of don't always come from the user system (they are often
// received from other machines) and so using wxFONTENCODING_SYSTEM doesn't
// seem to be a good idea and there is no other reasonable alternative
wxFontEncoding wxConvAuto::ms_defaultMBEncoding = wxFONTENCODING_ISO8859_1;

// ============================================================================
// implementation
// ============================================================================

/* static */
void wxConvAuto::SetFallbackEncoding(wxFontEncoding enc)
{
    wxASSERT_MSG( enc != wxFONTENCODING_DEFAULT,
                  wxT("wxFONTENCODING_DEFAULT doesn't make sense here") );

    ms_defaultMBEncoding = enc;
}

/* static */
wxConvAuto::BOMType wxConvAuto::DetectBOM(const char *src, size_t srcLen)
{
    // examine the buffer for BOM presence
    //
    // quoting from http://www.unicode.org/faq/utf_bom.html#BOM:
    //
    //  Bytes           Encoding Form
    //
    //  00 00 FE FF     UTF-32, big-endian
    //  FF FE 00 00     UTF-32, little-endian
    //  FE FF           UTF-16, big-endian
    //  FF FE           UTF-16, little-endian
    //  EF BB BF        UTF-8
    //
    // as some BOMs are prefixes of other ones we may need to read more bytes
    // to disambiguate them

    switch ( srcLen )
    {
        case 0:
            return BOM_Unknown;

        case 1:
            if ( src[0] == '\x00' || src[0] == '\xFF' ||
                 src[0] == '\xFE' || src[0] == '\xEF')
            {
                // this could be a BOM but we don't know yet
                return BOM_Unknown;
            }
            break;

        case 2:
        case 3:
            if ( src[0] == '\xEF' && src[1] == '\xBB' )
            {
                if ( srcLen == 3 )
                    return src[2] == '\xBF' ? BOM_UTF8 : BOM_None;

                return BOM_Unknown;
            }

            if ( src[0] == '\xFE' && src[1] == '\xFF' )
                return BOM_UTF16BE;

            if ( src[0] == '\xFF' && src[1] == '\xFE' )
            {
                // if the next byte is 0, it could be an UTF-32LE BOM but if it
                // isn't we can be sure it's UTF-16LE
                if ( srcLen == 3 && src[2] != '\x00' )
                    return BOM_UTF16LE;

                return BOM_Unknown;
            }

            if ( src[0] == '\x00' && src[1] == '\x00' )
            {
                // this could only be UTF-32BE, check that the data we have so
                // far allows for it
                if ( srcLen == 3 && src[2] != '\xFE' )
                    return BOM_None;

                return BOM_Unknown;
            }
            break;

        default:
            // we have at least 4 characters so we may finally decide whether
            // we have a BOM or not
            if ( src[0] == '\xEF' && src[1] == '\xBB' && src[2] == '\xBF' )
                return BOM_UTF8;

            if ( src[0] == '\x00' && src[1] == '\x00' &&
                 src[2] == '\xFE' && src[3] == '\xFF' )
                return BOM_UTF32BE;

            if ( src[0] == '\xFF' && src[1] == '\xFE' &&
                 src[2] == '\x00' && src[3] == '\x00' )
                return BOM_UTF32LE;

            if ( src[0] == '\xFE' && src[1] == '\xFF' )
                return BOM_UTF16BE;

            if ( src[0] == '\xFF' && src[1] == '\xFE' )
                return BOM_UTF16LE;
    }

    return BOM_None;
}

void wxConvAuto::InitFromBOM(BOMType bomType)
{
    m_consumedBOM = false;

    switch ( bomType )
    {
        case BOM_Unknown:
            wxFAIL_MSG( "shouldn't be called for this BOM type" );
            break;

        case BOM_None:
            // use the default
            break;

        case BOM_UTF32BE:
            m_conv = new wxMBConvUTF32BE;
            m_ownsConv = true;
            break;

        case BOM_UTF32LE:
            m_conv = new wxMBConvUTF32LE;
            m_ownsConv = true;
            break;

        case BOM_UTF16BE:
            m_conv = new wxMBConvUTF16BE;
            m_ownsConv = true;
            break;

        case BOM_UTF16LE:
            m_conv = new wxMBConvUTF16LE;
            m_ownsConv = true;
            break;

        case BOM_UTF8:
            InitWithUTF8();
            break;

        default:
            wxFAIL_MSG( "unknown BOM type" );
    }

    if ( !m_conv )
    {
        // we end up here if there is no BOM or we didn't recognize it somehow
        // (this shouldn't happen but still don't crash if it does), so use the
        // default encoding
        InitWithUTF8();
        m_consumedBOM = true; // as there is nothing to consume
    }
}

void wxConvAuto::SkipBOM(const char **src, size_t *len) const
{
    int ofs;
    switch ( m_bomType )
    {
        case BOM_Unknown:
            wxFAIL_MSG( "shouldn't be called for this BOM type" );
            return;

        case BOM_None:
            ofs = 0;
            break;

        case BOM_UTF32BE:
        case BOM_UTF32LE:
            ofs = 4;
            break;

        case BOM_UTF16BE:
        case BOM_UTF16LE:
            ofs = 2;
            break;

        case BOM_UTF8:
            ofs = 3;
            break;

        default:
            wxFAIL_MSG( "unknown BOM type" );
            return;
    }

    *src += ofs;
    if ( *len != (size_t)-1 )
        *len -= ofs;
}

bool wxConvAuto::InitFromInput(const char *src, size_t len)
{
    m_bomType = DetectBOM(src, len == wxNO_LEN ? strlen(src) : len);
    if ( m_bomType == BOM_Unknown )
        return false;

    InitFromBOM(m_bomType);

    return true;
}

size_t
wxConvAuto::ToWChar(wchar_t *dst, size_t dstLen,
                    const char *src, size_t srcLen) const
{
    // we check BOM and create the appropriate conversion the first time we're
    // called but we also need to ensure that the BOM is skipped not only
    // during this initial call but also during the first call with non-NULL
    // dst as typically we're first called with NULL dst to calculate the
    // needed buffer size
    wxConvAuto *self = const_cast<wxConvAuto *>(this);


    if ( !m_conv )
    {
        if ( !self->InitFromInput(src, srcLen) )
        {
            // there is not enough data to determine whether we have a BOM or
            // not, so fail for now -- the caller is supposed to call us again
            // with more data
            return wxCONV_FAILED;
        }
    }

    if ( !m_consumedBOM )
    {
        SkipBOM(&src, &srcLen);
        if ( srcLen == 0 )
        {
            // there is nothing left except the BOM so we'd return 0 below but
            // this is unexpected: decoding a non-empty string must either fail
            // or return something non-empty, in particular this would break
            // the code in wxTextInputStream::NextChar()
            //
            // so still return an error as we need some more data to be able to
            // decode it
            return wxCONV_FAILED;
        }
    }

    // try to convert using the auto-detected encoding
    size_t rc = m_conv->ToWChar(dst, dstLen, src, srcLen);
    if ( rc == wxCONV_FAILED && m_bomType == BOM_None )
    {
        // if the conversion failed but we didn't really detect anything and
        // simply tried UTF-8 by default, retry it using the fall-back
        if ( m_encDefault != wxFONTENCODING_MAX )
        {
            if ( m_ownsConv )
                delete m_conv;

            self->m_conv = new wxCSConv(m_encDefault == wxFONTENCODING_DEFAULT
                                            ? GetFallbackEncoding()
                                            : m_encDefault);
            self->m_ownsConv = true;

            rc = m_conv->ToWChar(dst, dstLen, src, srcLen);
        }
    }

    // don't skip the BOM again the next time if we really consumed it
    if ( rc != wxCONV_FAILED && dst && !m_consumedBOM )
        self->m_consumedBOM = true;

    return rc;
}

size_t
wxConvAuto::FromWChar(char *dst, size_t dstLen,
                      const wchar_t *src, size_t srcLen) const
{
    if ( !m_conv )
    {
        // default to UTF-8 for the multibyte output
        const_cast<wxConvAuto *>(this)->InitWithUTF8();
    }

    return m_conv->FromWChar(dst, dstLen, src, srcLen);
}
