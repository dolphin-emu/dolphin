/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/fontutil.cpp
// Purpose:     Font helper functions for wxX11, wxGTK, wxMotif
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.11.99
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/fontutil.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/font.h" // wxFont enums
    #include "wx/hash.h"
    #include "wx/utils.h"       // for wxGetDisplay()
    #include "wx/module.h"
#endif // PCH

#include "wx/encinfo.h"
#include "wx/fontmap.h"
#include "wx/tokenzr.h"
#include "wx/fontenum.h"

#if wxUSE_PANGO

#include <pango/pango.h>

PangoContext* wxGetPangoContext();

#ifdef __WXGTK20__
    #include "wx/gtk/private.h"

    #define wxPANGO_CONV wxGTK_CONV_SYS
    #define wxPANGO_CONV_BACK wxGTK_CONV_BACK_SYS
#else
    #include "wx/x11/private.h"
    #include "wx/gtk/private/string.h"

    #define wxPANGO_CONV(s) s.utf8_str()
    #define wxPANGO_CONV_BACK(s) wxString::FromUTF8Unchecked(s)
#endif

// ----------------------------------------------------------------------------
// wxNativeFontInfo
// ----------------------------------------------------------------------------

void wxNativeFontInfo::Init()
{
    description = NULL;
    m_underlined = false;
    m_strikethrough = false;
}

void wxNativeFontInfo::Init(const wxNativeFontInfo& info)
{
    if (info.description)
    {
        description = pango_font_description_copy(info.description);
        m_underlined = info.GetUnderlined();
        m_strikethrough = info.GetStrikethrough();
    }
    else
    {
        description = NULL;
        m_underlined = false;
        m_strikethrough = false;
    }
}

void wxNativeFontInfo::Free()
{
    if (description)
        pango_font_description_free(description);
}

int wxNativeFontInfo::GetPointSize() const
{
    return pango_font_description_get_size( description ) / PANGO_SCALE;
}

wxFontStyle wxNativeFontInfo::GetStyle() const
{
    wxFontStyle m_style = wxFONTSTYLE_NORMAL;

    switch (pango_font_description_get_style( description ))
    {
        case PANGO_STYLE_NORMAL:
            m_style = wxFONTSTYLE_NORMAL;
            break;
        case PANGO_STYLE_ITALIC:
            m_style = wxFONTSTYLE_ITALIC;
            break;
        case PANGO_STYLE_OBLIQUE:
            m_style = wxFONTSTYLE_SLANT;
            break;
    }

    return m_style;
}

wxFontWeight wxNativeFontInfo::GetWeight() const
{
    // We seem to currently initialize only by string.
    // In that case PANGO_FONT_MASK_WEIGHT is always set.
    // if (!(pango_font_description_get_set_fields(description) & PANGO_FONT_MASK_WEIGHT))
    //    return wxFONTWEIGHT_NORMAL;

    PangoWeight pango_weight = pango_font_description_get_weight( description );

    // Until the API can be changed the following ranges of weight values are used:
    // wxFONTWEIGHT_LIGHT:  100 .. 349 - range of 250
    // wxFONTWEIGHT_NORMAL: 350 .. 599 - range of 250
    // wxFONTWEIGHT_BOLD:   600 .. 900 - range of 301 (600 is "semibold" already)

    if (pango_weight >= 600)
        return wxFONTWEIGHT_BOLD;

    if (pango_weight < 350)
        return wxFONTWEIGHT_LIGHT;

    return wxFONTWEIGHT_NORMAL;
}

bool wxNativeFontInfo::GetUnderlined() const
{
    return m_underlined;
}

bool wxNativeFontInfo::GetStrikethrough() const
{
    return m_strikethrough;
}

wxString wxNativeFontInfo::GetFaceName() const
{
    // the Pango "family" is the wx "face name"
    return wxPANGO_CONV_BACK(pango_font_description_get_family(description));
}

wxFontFamily wxNativeFontInfo::GetFamily() const
{
    wxFontFamily ret = wxFONTFAMILY_UNKNOWN;

    const char *family_name = pango_font_description_get_family( description );

    // note: not passing -1 as the 2nd parameter to g_ascii_strdown to work
    // around a bug in the 64-bit glib shipped with solaris 10, -1 causes it
    // to try to allocate 2^32 bytes.
    if ( !family_name )
        return ret;
    wxGtkString family_text(g_ascii_strdown(family_name, strlen(family_name)));

    // Check for some common fonts, to salvage what we can from the current
    // win32 centric wxFont API:
    if (wxStrnicmp( family_text, "monospace", 9 ) == 0)
        ret = wxFONTFAMILY_TELETYPE;    // begins with "Monospace"
    else if (wxStrnicmp( family_text, "courier", 7 ) == 0)
        ret = wxFONTFAMILY_TELETYPE;    // begins with "Courier"
#if defined(__WXGTK20__) || defined(HAVE_PANGO_FONT_FAMILY_IS_MONOSPACE)
    else
    {
        PangoFontFamily **families;
        PangoFontFamily  *family = NULL;
        int n_families;
        PangoContext* context = wxGetPangoContext();
        pango_context_list_families(context, &families, &n_families);

        for (int i = 0; i < n_families; ++i)
        {
            if (g_ascii_strcasecmp(pango_font_family_get_name( families[i] ),
                                   pango_font_description_get_family( description )) == 0 )
            {
                family = families[i];
                break;
            }
        }

        g_free(families);
        g_object_unref(context);

        // Some gtk+ systems might query for a non-existing font from
        // wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT) on initialization,
        // don't assert until wxSystemSettings::GetFont is checked for this - MR
        // wxASSERT_MSG( family, "No appropriate PangoFontFamily found for ::description" );

        if (family != NULL && pango_font_family_is_monospace( family ))
            ret = wxFONTFAMILY_TELETYPE; // is deemed a monospace font by pango
    }
#endif // GTK+ 2 || HAVE_PANGO_FONT_FAMILY_IS_MONOSPACE

    if (ret == wxFONTFAMILY_UNKNOWN)
    {
        if (strstr( family_text, "sans" ) != NULL || strstr( family_text, "Sans" ) != NULL)
            // checked before serif, so that "* Sans Serif" fonts are detected correctly
            ret = wxFONTFAMILY_SWISS;       // contains "Sans"
        else if (strstr( family_text, "serif" ) != NULL || strstr( family_text, "Serif" ) != NULL)
            ret = wxFONTFAMILY_ROMAN;       // contains "Serif"
        else if (wxStrnicmp( family_text, "times", 5 ) == 0)
            ret = wxFONTFAMILY_ROMAN;       // begins with "Times"
        else if (wxStrnicmp( family_text, "old", 3 ) == 0)
            ret = wxFONTFAMILY_DECORATIVE;  // begins with "Old" - "Old English", "Old Town"
    }

    return ret;
}

wxFontEncoding wxNativeFontInfo::GetEncoding() const
{
    return wxFONTENCODING_SYSTEM;
}

void wxNativeFontInfo::SetPointSize(int pointsize)
{
    pango_font_description_set_size( description, pointsize * PANGO_SCALE );
}

void wxNativeFontInfo::SetStyle(wxFontStyle style)
{
    switch (style)
    {
        case wxFONTSTYLE_ITALIC:
            pango_font_description_set_style( description, PANGO_STYLE_ITALIC );
            break;
        case wxFONTSTYLE_SLANT:
            pango_font_description_set_style( description, PANGO_STYLE_OBLIQUE );
            break;
        default:
            wxFAIL_MSG( "unknown font style" );
            // fall through
        case wxFONTSTYLE_NORMAL:
            pango_font_description_set_style( description, PANGO_STYLE_NORMAL );
            break;
    }
}

void wxNativeFontInfo::SetWeight(wxFontWeight weight)
{
    switch (weight)
    {
        case wxFONTWEIGHT_BOLD:
            pango_font_description_set_weight(description, PANGO_WEIGHT_BOLD);
            break;
        case wxFONTWEIGHT_LIGHT:
            pango_font_description_set_weight(description, PANGO_WEIGHT_LIGHT);
            break;
        default:
            wxFAIL_MSG( "unknown font weight" );
            // fall through
        case wxFONTWEIGHT_NORMAL:
            pango_font_description_set_weight(description, PANGO_WEIGHT_NORMAL);
    }
}

void wxNativeFontInfo::SetUnderlined(bool underlined)
{
    // Pango doesn't have the underlined attribute so we store it separately
    // (and handle it specially in wxWindowDCImpl::DoDrawText()).
    m_underlined = underlined;
}

void wxNativeFontInfo::SetStrikethrough(bool strikethrough)
{
    // As with the underlined attribute above, we handle this one separately as
    // Pango doesn't support it as part of the font description.
    m_strikethrough = strikethrough;
}

bool wxNativeFontInfo::SetFaceName(const wxString& facename)
{
    pango_font_description_set_family(description, wxPANGO_CONV(facename));

    // we return true because Pango doesn't tell us if the call failed or not;
    // instead on wxGTK wxFont::SetFaceName() will call wxFontBase::SetFaceName()
    // which does the check
    return true;
}

void wxNativeFontInfo::SetFamily(wxFontFamily family)
{
    wxArrayString facename;

    // the list of fonts associated with a family was partially
    // taken from http://www.codestyle.org/css/font-family

    switch ( family )
    {
        case wxFONTFAMILY_SCRIPT:
            // corresponds to the cursive font family in the page linked above
            facename.Add(wxS("URW Chancery L"));
            facename.Add(wxS("Comic Sans MS"));
            break;

        case wxFONTFAMILY_DECORATIVE:
            // corresponds to the fantasy font family in the page linked above
            facename.Add(wxS("Impact"));
            break;

        case wxFONTFAMILY_ROMAN:
            // corresponds to the serif font family in the page linked above
            facename.Add(wxS("Serif"));
            facename.Add(wxS("DejaVu Serif"));
            facename.Add(wxS("DejaVu LGC Serif"));
            facename.Add(wxS("Bitstream Vera Serif"));
            facename.Add(wxS("Liberation Serif"));
            facename.Add(wxS("FreeSerif"));
            facename.Add(wxS("Luxi Serif"));
            facename.Add(wxS("Times New Roman"));
            facename.Add(wxS("Century Schoolbook L"));
            facename.Add(wxS("URW Bookman L"));
            facename.Add(wxS("URW Palladio L"));
            facename.Add(wxS("Times"));
            break;

        case wxFONTFAMILY_TELETYPE:
        case wxFONTFAMILY_MODERN:
            // corresponds to the monospace font family in the page linked above
            facename.Add(wxS("Monospace"));
            facename.Add(wxS("DejaVu Sans Mono"));
            facename.Add(wxS("DejaVu LGC Sans Mono"));
            facename.Add(wxS("Bitstream Vera Sans Mono"));
            facename.Add(wxS("Liberation Mono"));
            facename.Add(wxS("FreeMono"));
            facename.Add(wxS("Luxi Mono"));
            facename.Add(wxS("Courier New"));
            facename.Add(wxS("Lucida Sans Typewriter"));
            facename.Add(wxS("Nimbus Mono L"));
            facename.Add(wxS("Andale Mono"));
            facename.Add(wxS("Courier"));
            break;

        case wxFONTFAMILY_SWISS:
        case wxFONTFAMILY_DEFAULT:
        default:
            // corresponds to the sans-serif font family in the page linked above
            facename.Add(wxS("Sans"));
            facename.Add(wxS("DejaVu Sans"));
            facename.Add(wxS("DejaVu LGC Sans"));
            facename.Add(wxS("Bitstream Vera Sans"));
            facename.Add(wxS("Liberation Sans"));
            facename.Add(wxS("FreeSans"));
            facename.Add(wxS("Luxi Sans"));
            facename.Add(wxS("Arial"));
            facename.Add(wxS("Lucida Sans"));
            facename.Add(wxS("Nimbus Sans L"));
            facename.Add(wxS("URW Gothic L"));
            break;
    }

    SetFaceName(facename);
}

void wxNativeFontInfo::SetEncoding(wxFontEncoding WXUNUSED(encoding))
{
    wxFAIL_MSG( "not implemented: Pango encoding is always UTF8" );
}

bool wxNativeFontInfo::FromString(const wxString& s)
{
    wxString str(s);

    // Pango font description doesn't have 'underlined' or 'strikethrough'
    // attributes, so we handle them specially by extracting them from the
    // string before passing it to Pango.
    m_underlined = str.StartsWith(wxS("underlined "), &str);
    m_strikethrough = str.StartsWith(wxS("strikethrough "), &str);

    if (description)
        pango_font_description_free( description );

    // there is a bug in at least pango <= 1.13 which makes it (or its backends)
    // segfault for very big point sizes and for negative point sizes.
    // To workaround that bug for pango <= 1.13
    // (see http://bugzilla.gnome.org/show_bug.cgi?id=340229)
    // we do the check on the size here using same (arbitrary) limits used by
    // pango > 1.13. Note that the segfault could happen also for pointsize
    // smaller than this limit !!
    const size_t pos = str.find_last_of(wxS(" "));
    double size;
    if ( pos != wxString::npos && wxString(str, pos + 1).ToDouble(&size) )
    {
        wxString sizeStr;
        if ( size < 1 )
            sizeStr = wxS("1");
        else if ( size >= 1E6 )
            sizeStr = wxS("1E6");

        if ( !sizeStr.empty() )
        {
            // replace the old size with the adjusted one
            str = wxString(s, 0, pos) + sizeStr;
        }
    }

    description = pango_font_description_from_string(wxPANGO_CONV(str));

#if wxUSE_FONTENUM
    // ensure a valid facename is selected
    if (!wxFontEnumerator::IsValidFacename(GetFaceName()))
        SetFaceName(wxNORMAL_FONT->GetFaceName());
#endif // wxUSE_FONTENUM

    return true;
}

wxString wxNativeFontInfo::ToString() const
{
    wxGtkString str(pango_font_description_to_string( description ));
    wxString desc = wxPANGO_CONV_BACK(str);

    // Augment the string with the attributes not handled by Pango.
    //
    // Notice that we must add them in the same order they are extracted in
    // FromString() above.
    if (m_strikethrough)
        desc.insert(0, wxS("strikethrough "));
    if (m_underlined)
        desc.insert(0, wxS("underlined "));

    return desc;
}

bool wxNativeFontInfo::FromUserString(const wxString& s)
{
    return FromString( s );
}

wxString wxNativeFontInfo::ToUserString() const
{
    return ToString();
}

#else // GTK+ 1.x

#ifdef __X__
    #ifdef __VMS__
        #pragma message disable nosimpint
    #endif

    #include <X11/Xlib.h>

    #ifdef __VMS__
        #pragma message enable nosimpint
    #endif

#elif defined(__WXGTK__)
    // we have to declare struct tm to avoid problems with first forward
    // declaring it in C code (glib.h included from gdk.h does it) and then
    // defining it when time.h is included from the headers below - this is
    // known not to work at least with Sun CC 6.01
    #include <time.h>

    #include <gdk/gdk.h>
#endif


// ----------------------------------------------------------------------------
// private data
// ----------------------------------------------------------------------------

static wxHashTable *g_fontHash = NULL;

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// define the functions to create and destroy native fonts for this toolkit
#ifdef __X__
    wxNativeFont wxLoadFont(const wxString& fontSpec)
    {
        return XLoadQueryFont((Display *)wxGetDisplay(), fontSpec);
    }

    inline void wxFreeFont(wxNativeFont font)
    {
        XFreeFont((Display *)wxGetDisplay(), (XFontStruct *)font);
    }
#elif defined(__WXGTK__)
    wxNativeFont wxLoadFont(const wxString& fontSpec)
    {
        // VZ: we should use gdk_fontset_load() instead of gdk_font_load()
        //     here to be able to display Japanese fonts correctly (at least
        //     this is what people report) but unfortunately doing it results
        //     in tons of warnings when using GTK with "normal" European
        //     languages and so we can't always do it and I don't know enough
        //     to determine when should this be done... (FIXME)
        return gdk_font_load( wxConvertWX2MB(fontSpec) );
    }

    inline void wxFreeFont(wxNativeFont font)
    {
        gdk_font_unref(font);
    }
#else
    #error "Unknown GUI toolkit"
#endif

static bool wxTestFontSpec(const wxString& fontspec);

static wxNativeFont wxLoadQueryFont(int pointSize,
                                    wxFontFamily family,
                                    wxFontStyle style,
                                    wxFontWeight weight,
                                    bool underlined,
                                    const wxString& facename,
                                    const wxString& xregistry,
                                    const wxString& xencoding,
                                    wxString* xFontName);

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxNativeEncodingInfo
// ----------------------------------------------------------------------------

// convert to/from the string representation: format is
//      encodingid;registry;encoding[;facename]
bool wxNativeEncodingInfo::FromString(const wxString& s)
{
    // use ";", not "-" because it may be part of encoding name
    wxStringTokenizer tokenizer(s, wxT(";"));

    wxString encid = tokenizer.GetNextToken();
    long enc;
    if ( !encid.ToLong(&enc) )
        return false;
    encoding = (wxFontEncoding)enc;

    xregistry = tokenizer.GetNextToken();
    if ( !xregistry )
        return false;

    xencoding = tokenizer.GetNextToken();
    if ( !xencoding )
        return false;

    // ok even if empty
    facename = tokenizer.GetNextToken();

    return true;
}

wxString wxNativeEncodingInfo::ToString() const
{
    wxString s;
    s << (long)encoding << wxT(';') << xregistry << wxT(';') << xencoding;
    if ( !facename.empty() )
    {
        s << wxT(';') << facename;
    }

    return s;
}

// ----------------------------------------------------------------------------
// wxNativeFontInfo
// ----------------------------------------------------------------------------

void wxNativeFontInfo::Init()
{
    m_isDefault = true;
}

bool wxNativeFontInfo::FromString(const wxString& s)
{
    wxStringTokenizer tokenizer(s, wxT(";"));

    // check the version
    wxString token = tokenizer.GetNextToken();
    if ( token != wxT('0') )
        return false;

    xFontName = tokenizer.GetNextToken();

    // this should be the end
    if ( tokenizer.HasMoreTokens() )
        return false;

    return FromXFontName(xFontName);
}

wxString wxNativeFontInfo::ToString() const
{
    // 0 is the version
    return wxString::Format(wxT("%d;%s"), 0, GetXFontName().c_str());
}

bool wxNativeFontInfo::FromUserString(const wxString& s)
{
    return FromXFontName(s);
}

wxString wxNativeFontInfo::ToUserString() const
{
    return GetXFontName();
}

bool wxNativeFontInfo::HasElements() const
{
    // we suppose that the foundry is never empty, so if it is it means that we
    // had never parsed the XLFD
    return !fontElements[0].empty();
}

wxString wxNativeFontInfo::GetXFontComponent(wxXLFDField field) const
{
    wxCHECK_MSG( field < wxXLFD_MAX, wxEmptyString, wxT("invalid XLFD field") );

    if ( !HasElements() )
    {
        if ( !const_cast<wxNativeFontInfo *>(this)->FromXFontName(xFontName) )
            return wxEmptyString;
    }

    return fontElements[field];
}

bool wxNativeFontInfo::FromXFontName(const wxString& fontname)
{
    // TODO: we should be able to handle the font aliases here, but how?
    wxStringTokenizer tokenizer(fontname, wxT("-"));

    // skip the leading, usually empty field (font name registry)
    if ( !tokenizer.HasMoreTokens() )
        return false;

    (void)tokenizer.GetNextToken();

    for ( size_t n = 0; n < WXSIZEOF(fontElements); n++ )
    {
        if ( !tokenizer.HasMoreTokens() )
        {
            // not enough elements in the XLFD - or maybe an alias
            return false;
        }

        wxString field = tokenizer.GetNextToken();
        if ( !field.empty() && field != wxT('*') )
        {
            // we're really initialized now
            m_isDefault = false;
        }

        fontElements[n] = field;
    }

    // this should be all
    if ( tokenizer.HasMoreTokens() )
        return false;

    return true;
}

wxString wxNativeFontInfo::GetXFontName() const
{
    if ( xFontName.empty() )
    {
        for ( size_t n = 0; n < WXSIZEOF(fontElements); n++ )
        {
            // replace the non specified elements with '*' except for the
            // additional style which is usually just omitted
            wxString elt = fontElements[n];
            if ( elt.empty() && n != wxXLFD_ADDSTYLE )
            {
                elt = wxT('*');
            }

            const_cast<wxNativeFontInfo *>(this)->xFontName << wxT('-') << elt;
        }
    }

    return xFontName;
}

void
wxNativeFontInfo::SetXFontComponent(wxXLFDField field, const wxString& value)
{
    wxCHECK_RET( field < wxXLFD_MAX, wxT("invalid XLFD field") );

    // this class should be initialized with a valid font spec first and only
    // then the fields may be modified!
    wxASSERT_MSG( !IsDefault(), wxT("can't modify an uninitialized XLFD") );

    if ( !HasElements() )
    {
        if ( !const_cast<wxNativeFontInfo *>(this)->FromXFontName(xFontName) )
        {
            wxFAIL_MSG( wxT("can't set font element for invalid XLFD") );

            return;
        }
    }

    fontElements[field] = value;

    // invalidate the XFLD, it doesn't correspond to the font elements any more
    xFontName.clear();
}

void wxNativeFontInfo::SetXFontName(const wxString& xFontName_)
{
    // invalidate the font elements, GetXFontComponent() will reparse the XLFD
    fontElements[0].clear();

    xFontName = xFontName_;

    m_isDefault = false;
}

int wxNativeFontInfo::GetPointSize() const
{
    const wxString s = GetXFontComponent(wxXLFD_POINTSIZE);

    // return -1 to indicate that the size is unknown
    long l;
    return s.ToLong(&l) ? l : -1;
}

wxFontStyle wxNativeFontInfo::GetStyle() const
{
    const wxString s = GetXFontComponent(wxXLFD_SLANT);

    if ( s.length() != 1 )
    {
        // it is really unknown but we don't have any way to return it from
        // here
        return wxFONTSTYLE_NORMAL;
    }

    switch ( s[0].GetValue() )
    {
        default:
            // again, unknown but consider normal by default

        case wxT('r'):
            return wxFONTSTYLE_NORMAL;

        case wxT('i'):
            return wxFONTSTYLE_ITALIC;

        case wxT('o'):
            return wxFONTSTYLE_SLANT;
    }
}

wxFontWeight wxNativeFontInfo::GetWeight() const
{
    const wxString s = GetXFontComponent(wxXLFD_WEIGHT).MakeLower();
    if ( s.find(wxT("bold")) != wxString::npos || s == wxT("black") )
        return wxFONTWEIGHT_BOLD;
    else if ( s == wxT("light") )
        return wxFONTWEIGHT_LIGHT;

    return wxFONTWEIGHT_NORMAL;
}

bool wxNativeFontInfo::GetUnderlined() const
{
    // X fonts are never underlined
    return false;
}

wxString wxNativeFontInfo::GetFaceName() const
{
    // wxWidgets facename probably more accurately corresponds to X family
    return GetXFontComponent(wxXLFD_FAMILY);
}

wxFontFamily wxNativeFontInfo::GetFamily() const
{
    // and wxWidgets family -- to X foundry, but we have to translate it to
    // wxFontFamily somehow...
    wxFAIL_MSG(wxT("not implemented")); // GetXFontComponent(wxXLFD_FOUNDRY);

    return wxFONTFAMILY_DEFAULT;
}

wxFontEncoding wxNativeFontInfo::GetEncoding() const
{
    // we already have the code for this but need to refactor it first
    wxFAIL_MSG( wxT("not implemented") );

    return wxFONTENCODING_MAX;
}

void wxNativeFontInfo::SetPointSize(int pointsize)
{
    SetXFontComponent(wxXLFD_POINTSIZE, wxString::Format(wxT("%d"), pointsize));
}

void wxNativeFontInfo::SetStyle(wxFontStyle style)
{
    wxString s;
    switch ( style )
    {
        case wxFONTSTYLE_ITALIC:
            s = wxT('i');
            break;

        case wxFONTSTYLE_SLANT:
            s = wxT('o');
            break;

        case wxFONTSTYLE_NORMAL:
            s = wxT('r');

        default:
            wxFAIL_MSG( wxT("unknown wxFontStyle in wxNativeFontInfo::SetStyle") );
            return;
    }

    SetXFontComponent(wxXLFD_SLANT, s);
}

void wxNativeFontInfo::SetWeight(wxFontWeight weight)
{
    wxString s;
    switch ( weight )
    {
        case wxFONTWEIGHT_BOLD:
            s = wxT("bold");
            break;

        case wxFONTWEIGHT_LIGHT:
            s = wxT("light");
            break;

        case wxFONTWEIGHT_NORMAL:
            s = wxT("medium");
            break;

        default:
            wxFAIL_MSG( wxT("unknown wxFontWeight in wxNativeFontInfo::SetWeight") );
            return;
    }

    SetXFontComponent(wxXLFD_WEIGHT, s);
}

void wxNativeFontInfo::SetUnderlined(bool WXUNUSED(underlined))
{
    // can't do this under X
}

void wxNativeFontInfo::SetStrikethrough(bool WXUNUSED(strikethrough))
{
    // this is not supported by Pango fonts neither
}

bool wxNativeFontInfo::SetFaceName(const wxString& facename)
{
    SetXFontComponent(wxXLFD_FAMILY, facename);
    return true;
}

void wxNativeFontInfo::SetFamily(wxFontFamily WXUNUSED(family))
{
    // wxFontFamily -> X foundry, anyone?
    wxFAIL_MSG( wxT("not implemented") );

    // SetXFontComponent(wxXLFD_FOUNDRY, ...);
}

void wxNativeFontInfo::SetEncoding(wxFontEncoding encoding)
{
    wxNativeEncodingInfo info;
    if ( wxGetNativeFontEncoding(encoding, &info) )
    {
        SetXFontComponent(wxXLFD_ENCODING, info.xencoding);
        SetXFontComponent(wxXLFD_REGISTRY, info.xregistry);
    }
}

// ----------------------------------------------------------------------------
// common functions
// ----------------------------------------------------------------------------

bool wxGetNativeFontEncoding(wxFontEncoding encoding,
                             wxNativeEncodingInfo *info)
{
    wxCHECK_MSG( info, false, wxT("bad pointer in wxGetNativeFontEncoding") );

    if ( encoding == wxFONTENCODING_DEFAULT )
    {
        encoding = wxFont::GetDefaultEncoding();
    }

    switch ( encoding )
    {
        case wxFONTENCODING_ISO8859_1:
        case wxFONTENCODING_ISO8859_2:
        case wxFONTENCODING_ISO8859_3:
        case wxFONTENCODING_ISO8859_4:
        case wxFONTENCODING_ISO8859_5:
        case wxFONTENCODING_ISO8859_6:
        case wxFONTENCODING_ISO8859_7:
        case wxFONTENCODING_ISO8859_8:
        case wxFONTENCODING_ISO8859_9:
        case wxFONTENCODING_ISO8859_10:
        case wxFONTENCODING_ISO8859_11:
        case wxFONTENCODING_ISO8859_12:
        case wxFONTENCODING_ISO8859_13:
        case wxFONTENCODING_ISO8859_14:
        case wxFONTENCODING_ISO8859_15:
            {
                int cp = encoding - wxFONTENCODING_ISO8859_1 + 1;
                info->xregistry = wxT("iso8859");
                info->xencoding.Printf(wxT("%d"), cp);
            }
            break;

        case wxFONTENCODING_UTF8:
            info->xregistry = wxT("iso10646");
            info->xencoding = wxT("*");
            break;

        case wxFONTENCODING_GB2312:
            info->xregistry = wxT("GB2312");   // or the otherway round?
            info->xencoding = wxT("*");
            break;

        case wxFONTENCODING_KOI8:
        case wxFONTENCODING_KOI8_U:
            info->xregistry = wxT("koi8");

            // we don't make distinction between koi8-r, koi8-u and koi8-ru (so far)
            info->xencoding = wxT("*");
            break;

        case wxFONTENCODING_CP1250:
        case wxFONTENCODING_CP1251:
        case wxFONTENCODING_CP1252:
        case wxFONTENCODING_CP1253:
        case wxFONTENCODING_CP1254:
        case wxFONTENCODING_CP1255:
        case wxFONTENCODING_CP1256:
        case wxFONTENCODING_CP1257:
            {
                int cp = encoding - wxFONTENCODING_CP1250 + 1250;
                info->xregistry = wxT("microsoft");
                info->xencoding.Printf(wxT("cp%d"), cp);
            }
            break;

        case wxFONTENCODING_EUC_JP:
        case wxFONTENCODING_SHIFT_JIS:
            info->xregistry = "jis*";
            info->xencoding = "*";
            break;

        case wxFONTENCODING_SYSTEM:
            info->xregistry =
            info->xencoding = wxT("*");
            break;

        default:
            // don't know how to translate this encoding into X fontspec
            return false;
    }

    info->encoding = encoding;

    return true;
}

bool wxTestFontEncoding(const wxNativeEncodingInfo& info)
{
    wxString fontspec;
    fontspec.Printf(wxT("-*-%s-*-*-*-*-*-*-*-*-*-*-%s-%s"),
                    info.facename.empty() ? wxString("*") : info.facename,
                    info.xregistry,
                    info.xencoding);

    return wxTestFontSpec(fontspec);
}

// ----------------------------------------------------------------------------
// X-specific functions
// ----------------------------------------------------------------------------

wxNativeFont wxLoadQueryNearestFont(int pointSize,
                                    wxFontFamily family,
                                    wxFontStyle style,
                                    wxFontWeight weight,
                                    bool underlined,
                                    const wxString &facename,
                                    wxFontEncoding encoding,
                                    wxString* xFontName)
{
    if ( encoding == wxFONTENCODING_DEFAULT )
    {
        encoding = wxFont::GetDefaultEncoding();
    }

    // first determine the encoding - if the font doesn't exist at all in this
    // encoding, it's useless to do all other approximations (i.e. size,
    // family &c don't matter much)
    wxNativeEncodingInfo info;
    if ( encoding == wxFONTENCODING_SYSTEM )
    {
        // This will always work so we don't test to save time
        wxGetNativeFontEncoding(wxFONTENCODING_SYSTEM, &info);
    }
    else
    {
        if ( !wxGetNativeFontEncoding(encoding, &info) ||
             !wxTestFontEncoding(info) )
        {
#if wxUSE_FONTMAP
            if ( !wxFontMapper::Get()->GetAltForEncoding(encoding, &info) )
#endif // wxUSE_FONTMAP
            {
                // unspported encoding - replace it with the default
                //
                // NB: we can't just return 0 from here because wxGTK code doesn't
                //     check for it (i.e. it supposes that we'll always succeed),
                //     so it would provoke a crash
                wxGetNativeFontEncoding(wxFONTENCODING_SYSTEM, &info);
            }
        }
    }

    // OK, we have the correct xregistry/xencoding in info structure
    wxNativeFont font = 0;

    // if we already have the X font name, try to use it
    if( xFontName && !xFontName->empty() )
    {
        //
        //  Make sure point size is correct for scale factor.
        //
        wxStringTokenizer tokenizer(*xFontName, wxT("-"), wxTOKEN_RET_DELIMS);
        wxString newFontName;

        for(int i = 0; i < 8; i++)
          newFontName += tokenizer.NextToken();

        (void) tokenizer.NextToken();

        newFontName += wxString::Format(wxT("%d-"), pointSize);

        while(tokenizer.HasMoreTokens())
          newFontName += tokenizer.GetNextToken();

        font = wxLoadFont(newFontName);

        if(font)
          *xFontName = newFontName;
    }

    if ( !font )
    {
        // search up and down by stepsize 10
        int max_size = pointSize + 20 * (1 + (pointSize/180));
        int min_size = pointSize - 20 * (1 + (pointSize/180));

        int i, round; // counters

        // first round: search for equal, then for smaller and for larger size
        // with the given weight and style
        wxFontWeight testweight = weight;
        wxFontStyle teststyle = style;

        for ( round = 0; round < 3; round++ )
        {
            // second round: use normal weight
            if ( round == 1 )
            {
                if ( testweight != wxFONTWEIGHT_NORMAL )
                {
                    testweight = wxFONTWEIGHT_NORMAL;
                }
                else
                {
                    ++round; // fall through to third round
                }
            }

            // third round: ... and use normal style
            if ( round == 2 )
            {
                if ( teststyle != wxFONTSTYLE_NORMAL )
                {
                    teststyle = wxFONTSTYLE_NORMAL;
                }
                else
                {
                    break;
                }
            }
            // Search for equal or smaller size (approx.)
            for ( i = pointSize; !font && i >= 10 && i >= min_size; i -= 10 )
            {
                font = wxLoadQueryFont(i, family, teststyle, testweight, underlined,
                                   facename, info.xregistry, info.xencoding,
                                   xFontName);
            }

            // Search for larger size (approx.)
            for ( i = pointSize + 10; !font && i <= max_size; i += 10 )
            {
                font = wxLoadQueryFont(i, family, teststyle, testweight, underlined,
                                   facename, info.xregistry, info.xencoding,
                                   xFontName);
            }
        }

        // Try default family
        if ( !font && family != wxFONTFAMILY_DEFAULT )
        {
            font = wxLoadQueryFont(pointSize, wxFONTFAMILY_DEFAULT, style, weight,
                                   underlined, facename,
                                   info.xregistry, info.xencoding,
                                   xFontName );
        }

        // ignore size, family, style and weight but try to find font with the
        // given facename and encoding
        if ( !font )
        {
            font = wxLoadQueryFont(120, wxFONTFAMILY_DEFAULT,
                                   wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                                   underlined, facename,
                                   info.xregistry, info.xencoding,
                                   xFontName);

            // ignore family as well
            if ( !font )
            {
                font = wxLoadQueryFont(120, wxFONTFAMILY_DEFAULT,
                                       wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                                       underlined, wxEmptyString,
                                       info.xregistry, info.xencoding,
                                       xFontName);

                // if it still failed, try to get the font of any size but
                // with the requested encoding: this can happen if the
                // encoding is only available in one size which happens to be
                // different from 120
                if ( !font )
                {
                    font = wxLoadQueryFont(-1, wxFONTFAMILY_DEFAULT,
                                           wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                                           false, wxEmptyString,
                                           info.xregistry, info.xencoding,
                                           xFontName);

                    // this should never happen as we had tested for it in the
                    // very beginning, but if it does, do return something non
                    // NULL or we'd crash in wxFont code
                    if ( !font )
                    {
                        wxFAIL_MSG( wxT("this encoding should be available!") );

                        font = wxLoadQueryFont(-1, wxFONTFAMILY_DEFAULT,
                                               wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                                               false, wxEmptyString,
                                               wxT("*"), wxT("*"),
                                               xFontName);
                    }
                }
            }
        }
    }

    return font;
}

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// returns true if there are any fonts matching this font spec
static bool wxTestFontSpec(const wxString& fontspec)
{
    // some X servers will fail to load this font because there are too many
    // matches so we must test explicitly for this
    if ( fontspec == wxT("-*-*-*-*-*-*-*-*-*-*-*-*-*-*") )
    {
        return true;
    }

    wxNativeFont test = (wxNativeFont) g_fontHash->Get( fontspec );
    if (test)
    {
        return true;
    }

    test = wxLoadFont(fontspec);
    g_fontHash->Put( fontspec, (wxObject*) test );

    if ( test )
    {
        wxFreeFont(test);

        return true;
    }
    else
    {
        return false;
    }
}

static wxNativeFont wxLoadQueryFont(int pointSize,
                                    wxFontFamily family,
                                    wxFontStyle style,
                                    wxFontWeight weight,
                                    bool WXUNUSED(underlined),
                                    const wxString& facename,
                                    const wxString& xregistry,
                                    const wxString& xencoding,
                                    wxString* xFontName)
{
    wxString xfamily("*");
    switch (family)
    {
        case wxFONTFAMILY_DECORATIVE: xfamily = wxT("lucida"); break;
        case wxFONTFAMILY_ROMAN:      xfamily = wxT("times");  break;
        case wxFONTFAMILY_MODERN:     xfamily = wxT("courier"); break;
        case wxFONTFAMILY_SWISS:      xfamily = wxT("helvetica"); break;
        case wxFONTFAMILY_TELETYPE:   xfamily = wxT("lucidatypewriter"); break;
        case wxFONTFAMILY_SCRIPT:     xfamily = wxT("utopia"); break;
    }
#if wxUSE_NANOX
    int xweight;
    switch (weight)
    {
         case wxFONTWEIGHT_BOLD:
             {
                 xweight = MWLF_WEIGHT_BOLD;
                 break;
             }
        case wxFONTWEIGHT_LIGHT:
             {
                 xweight = MWLF_WEIGHT_LIGHT;
                 break;
             }
         case wxFONTWEIGHT_NORMAL:
             {
                 xweight = MWLF_WEIGHT_NORMAL;
                 break;
             }

     default:
             {
                 xweight = MWLF_WEIGHT_DEFAULT;
                 break;
             }
    }
    GR_SCREEN_INFO screenInfo;
    GrGetScreenInfo(& screenInfo);

    int yPixelsPerCM = screenInfo.ydpcm;

    // A point is 1/72 of an inch.
    // An inch is 2.541 cm.
    // So pixelHeight = (pointSize / 72) (inches) * 2.541 (for cm) * yPixelsPerCM (for pixels)
    // In fact pointSize is 10 * the normal point size so
    // divide by 10.

    int pixelHeight = (int) ( (((float)pointSize) / 720.0) * 2.541 * (float) yPixelsPerCM) ;

    // An alternative: assume that the screen is 72 dpi.
    //int pixelHeight = (int) (((float)pointSize / 720.0) * 72.0) ;
    //int pixelHeight = (int) ((float)pointSize / 10.0) ;

    GR_LOGFONT logFont;
    logFont.lfHeight = pixelHeight;
    logFont.lfWidth = 0;
    logFont.lfEscapement = 0;
    logFont.lfOrientation = 0;
    logFont.lfWeight = xweight;
    logFont.lfItalic = (style == wxFONTSTYLE_ITALIC ? 0 : 1) ;
    logFont.lfUnderline = 0;
    logFont.lfStrikeOut = 0;
    logFont.lfCharSet = MWLF_CHARSET_DEFAULT; // TODO: select appropriate one
    logFont.lfOutPrecision = MWLF_TYPE_DEFAULT;
    logFont.lfClipPrecision = 0; // Not used
    logFont.lfRoman = (family == wxROMAN ? 1 : 0) ;
    logFont.lfSerif = (family == wxSWISS ? 0 : 1) ;
    logFont.lfSansSerif = !logFont.lfSerif ;
    logFont.lfModern = (family == wxMODERN ? 1 : 0) ;
    logFont.lfProportional = (family == wxTELETYPE ? 0 : 1) ;
    logFont.lfOblique = 0;
    logFont.lfSmallCaps = 0;
    logFont.lfPitch = 0; // 0 = default
    strcpy(logFont.lfFaceName, facename.c_str());

    XFontStruct* fontInfo = (XFontStruct*) malloc(sizeof(XFontStruct));
    fontInfo->fid = GrCreateFont((GR_CHAR*) facename.c_str(), pixelHeight, & logFont);
    GrGetFontInfo(fontInfo->fid, & fontInfo->info);
    return (wxNativeFont) fontInfo;

#else
    wxString fontSpec;
    if (!facename.empty())
    {
        fontSpec.Printf(wxT("-*-%s-*-*-normal-*-*-*-*-*-*-*-*-*"),
                        facename.c_str());

        if ( wxTestFontSpec(fontSpec) )
        {
            xfamily = facename;
        }
        //else: no such family, use default one instead
    }

    wxString xstyle;
    switch (style)
    {
        case wxFONTSTYLE_SLANT:
            fontSpec.Printf(wxT("-*-%s-*-o-*-*-*-*-*-*-*-*-*-*"),
                    xfamily.c_str());
            if ( wxTestFontSpec(fontSpec) )
            {
                xstyle = wxT("o");
                break;
            }
            // fall through - try wxFONTSTYLE_ITALIC now

        case wxFONTSTYLE_ITALIC:
            fontSpec.Printf(wxT("-*-%s-*-i-*-*-*-*-*-*-*-*-*-*"),
                    xfamily.c_str());
            if ( wxTestFontSpec(fontSpec) )
            {
                xstyle = wxT("i");
            }
            else if ( style == wxFONTSTYLE_ITALIC ) // and not wxFONTSTYLE_SLANT
            {
                // try wxFONTSTYLE_SLANT
                fontSpec.Printf(wxT("-*-%s-*-o-*-*-*-*-*-*-*-*-*-*"),
                        xfamily.c_str());
                if ( wxTestFontSpec(fontSpec) )
                {
                    xstyle = wxT("o");
                }
                else
                {
                    // no italic, no slant - leave default
                    xstyle = wxT("*");
                }
            }
            break;

        default:
            wxFAIL_MSG(wxT("unknown font style"));
            // fall back to normal

        case wxFONTSTYLE_NORMAL:
            xstyle = wxT("r");
            break;
    }

    wxString xweight;
    switch (weight)
    {
         case wxFONTWEIGHT_BOLD:
             {
                  fontSpec.Printf(wxT("-*-%s-bold-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                       xweight = wxT("bold");
                       break;
                  }
                  fontSpec.Printf(wxT("-*-%s-heavy-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                       xweight = wxT("heavy");
                       break;
                  }
                  fontSpec.Printf(wxT("-*-%s-extrabold-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                      xweight = wxT("extrabold");
                      break;
                  }
                  fontSpec.Printf(wxT("-*-%s-demibold-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                      xweight = wxT("demibold");
                      break;
                  }
                  fontSpec.Printf(wxT("-*-%s-black-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                      xweight = wxT("black");
                      break;
                  }
                  fontSpec.Printf(wxT("-*-%s-ultrablack-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                      xweight = wxT("ultrablack");
                      break;
                  }
              }
              break;
        case wxFONTWEIGHT_LIGHT:
             {
                  fontSpec.Printf(wxT("-*-%s-light-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                       xweight = wxT("light");
                       break;
                  }
                  fontSpec.Printf(wxT("-*-%s-thin-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                       xweight = wxT("thin");
                       break;
                  }
             }
             break;
         case wxFONTWEIGHT_NORMAL:
             {
                  fontSpec.Printf(wxT("-*-%s-medium-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                       xweight = wxT("medium");
                       break;
                  }
                  fontSpec.Printf(wxT("-*-%s-normal-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                       xweight = wxT("normal");
                       break;
                  }
                  fontSpec.Printf(wxT("-*-%s-regular-*-*-*-*-*-*-*-*-*-*-*"),
                         xfamily.c_str());
                  if ( wxTestFontSpec(fontSpec) )
                  {
                      xweight = wxT("regular");
                      break;
                  }
                  xweight = wxT("*");
              }
              break;
        default:           xweight = wxT("*"); break;
    }

    // if pointSize is -1, don't specify any
    wxString sizeSpec;
    if ( pointSize == -1 )
    {
        sizeSpec = wxT('*');
    }
    else
    {
        sizeSpec.Printf(wxT("%d"), pointSize);
    }

    // construct the X font spec from our data
    fontSpec.Printf(wxT("-*-%s-%s-%s-normal-*-*-%s-*-*-*-*-%s-%s"),
                    xfamily.c_str(), xweight.c_str(), xstyle.c_str(),
                    sizeSpec.c_str(), xregistry.c_str(), xencoding.c_str());

    if( xFontName )
        *xFontName = fontSpec;

    return wxLoadFont(fontSpec);
#endif
    // wxUSE_NANOX
}

// ----------------------------------------------------------------------------
// wxFontModule
// ----------------------------------------------------------------------------

class wxFontModule : public wxModule
{
public:
    bool OnInit();
    void OnExit();

private:
    wxDECLARE_DYNAMIC_CLASS(wxFontModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxFontModule, wxModule);

bool wxFontModule::OnInit()
{
    g_fontHash = new wxHashTable( wxKEY_STRING );

    return true;
}

void wxFontModule::OnExit()
{
    wxDELETE(g_fontHash);
}

#endif // GTK 2.0/1.x
