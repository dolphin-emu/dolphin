///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/fontutil.cpp
// Purpose:     font-related helper functions for OS X
// Author:      Vadim Zeitlin, Stefan Csomor
// Modified by:
// Created:     05.11.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/wxcrtvararg.h"
    #include "wx/log.h"
    #include "wx/intl.h"
#endif

#include "wx/fontutil.h"
#include "wx/fontmap.h"
#include "wx/encinfo.h"
#include "wx/tokenzr.h"


// convert to/from the string representation:
// format is facename[;charset]
//
bool wxNativeEncodingInfo::FromString( const wxString& s )
{
    wxStringTokenizer tokenizer(s, wxT(";"));

    facename = tokenizer.GetNextToken();
    if ( !facename )
        return false;

    wxString tmp = tokenizer.GetNextToken();
    if ( !tmp )
    {
        // default charset (don't use DEFAULT_CHARSET though because of subtle
        // Windows 9x/NT differences in handling it)
        charset = 0;
    }
    else
    {
        if ( wxSscanf( tmp, wxT("%u"), &charset ) != 1 )
            // should be a number!
            return false;
    }

    return true;
}

wxString wxNativeEncodingInfo::ToString() const
{
    wxString s(facename);
    if ( charset != 0 )
        s << wxT(';') << charset;

    return s;
}

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

bool wxGetNativeFontEncoding( wxFontEncoding encoding, wxNativeEncodingInfo *info )
{
    wxCHECK_MSG( info, false, wxT("bad pointer in wxGetNativeFontEncoding") );

    if ( encoding == wxFONTENCODING_DEFAULT )
        encoding = wxFont::GetDefaultEncoding();

    info->encoding = encoding;

    return true;
}

bool wxTestFontEncoding( const wxNativeEncodingInfo& WXUNUSED(info) )
{
    // basically we should be able to support every encoding via the OS
    return true;
}
