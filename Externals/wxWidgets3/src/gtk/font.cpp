/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/font.cpp
// Purpose:     wxFont for wxGTK
// Author:      Robert Roebling
// Id:          $Id: font.cpp 66641 2011-01-07 22:01:22Z SC $
// Copyright:   (c) 1998 Robert Roebling and Julian Smart
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

#include "wx/font.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/settings.h"
    #include "wx/gdicmn.h"
#endif

#include "wx/fontutil.h"
#include "wx/tokenzr.h"

#include "wx/gtk/private.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the default size (in points) for the fonts
static const int wxDEFAULT_FONT_SIZE = 12;

// ----------------------------------------------------------------------------
// wxFontRefData
// ----------------------------------------------------------------------------

class wxFontRefData : public wxGDIRefData
{
public:
    // from broken down font parameters, also default ctor
    wxFontRefData(int size = -1,
                  wxFontFamily family = wxFONTFAMILY_DEFAULT,
                  wxFontStyle style = wxFONTSTYLE_NORMAL,
                  wxFontWeight weight = wxFONTWEIGHT_NORMAL,
                  bool underlined = false,
                  const wxString& faceName = wxEmptyString,
                  wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    wxFontRefData(const wxString& nativeFontInfoString);

    // copy ctor
    wxFontRefData( const wxFontRefData& data );

    virtual ~wxFontRefData();

    // setters: all of them also take care to modify m_nativeFontInfo if we
    // have it so as to not lose the information not carried by our fields
    void SetPointSize(int pointSize);
    void SetFamily(wxFontFamily family);
    void SetStyle(wxFontStyle style);
    void SetWeight(wxFontWeight weight);
    void SetUnderlined(bool underlined);
    bool SetFaceName(const wxString& facename);
    void SetEncoding(wxFontEncoding encoding);

    // and this one also modifies all the other font data fields
    void SetNativeFontInfo(const wxNativeFontInfo& info);

protected:
    // common part of all ctors
    void Init(int pointSize,
              wxFontFamily family,
              wxFontStyle style,
              wxFontWeight weight,
              bool underlined,
              const wxString& faceName,
              wxFontEncoding encoding);

    // set all fields from (already initialized and valid) m_nativeFontInfo
    void InitFromNative();

private:
    bool            m_underlined;

    // The native font info: basically a PangoFontDescription
    wxNativeFontInfo m_nativeFontInfo;

    friend class wxFont;
};

#define M_FONTDATA ((wxFontRefData*)m_refData)

// ----------------------------------------------------------------------------
// wxFontRefData
// ----------------------------------------------------------------------------

void wxFontRefData::Init(int pointSize,
                         wxFontFamily family,
                         wxFontStyle style,
                         wxFontWeight weight,
                         bool underlined,
                         const wxString& faceName,
                         wxFontEncoding WXUNUSED(encoding))
{
    if (family == wxFONTFAMILY_DEFAULT)
        family = wxFONTFAMILY_SWISS;

    m_underlined = underlined;

    // Create native font info
    m_nativeFontInfo.description = pango_font_description_new();

    // And set its values
    if (!faceName.empty())
    {
        pango_font_description_set_family( m_nativeFontInfo.description,
                                           wxGTK_CONV_SYS(faceName) );
    }
    else
    {
        SetFamily(family);
    }

    SetStyle( style == wxDEFAULT ? wxFONTSTYLE_NORMAL : style );
    SetPointSize( (pointSize == wxDEFAULT || pointSize == -1)
                    ? wxDEFAULT_FONT_SIZE
                    : pointSize );
    SetWeight( weight == wxDEFAULT ? wxFONTWEIGHT_NORMAL : weight );
}

void wxFontRefData::InitFromNative()
{
    // Get native info
    PangoFontDescription *desc = m_nativeFontInfo.description;

    // Pango sometimes needs to have a size
    int pango_size = pango_font_description_get_size( desc );
    if (pango_size == 0)
        m_nativeFontInfo.SetPointSize(wxDEFAULT_FONT_SIZE);

    // Pango description are never underlined
    m_underlined = false;
}

wxFontRefData::wxFontRefData( const wxFontRefData& data )
             : wxGDIRefData()
{
    m_underlined = data.m_underlined;

    // Forces a copy of the internal data.  wxNativeFontInfo should probably
    // have a copy ctor and assignment operator to fix this properly but that
    // would break binary compatibility...
    m_nativeFontInfo.FromString(data.m_nativeFontInfo.ToString());
}

wxFontRefData::wxFontRefData(int size, wxFontFamily family, wxFontStyle style,
                             wxFontWeight weight, bool underlined,
                             const wxString& faceName,
                             wxFontEncoding encoding)
{
    Init(size, family, style, weight, underlined, faceName, encoding);
}

wxFontRefData::wxFontRefData(const wxString& nativeFontInfoString)
{
    m_nativeFontInfo.FromString( nativeFontInfoString );

    InitFromNative();
}

wxFontRefData::~wxFontRefData()
{
}

// ----------------------------------------------------------------------------
// wxFontRefData SetXXX()
// ----------------------------------------------------------------------------

void wxFontRefData::SetPointSize(int pointSize)
{
    m_nativeFontInfo.SetPointSize(pointSize);
}

/*
    NOTE: disabled because pango_font_description_set_absolute_size() and
          wxDC::GetCharHeight() do not mix well: setting with the former a pixel
          size of "30" makes the latter return 36...
          Besides, we need to return GetPointSize() a point size value even if
          SetPixelSize() was used and this would require further changes
          (and use of pango_font_description_get_size_is_absolute in some places).

bool wxFontRefData::SetPixelSize(const wxSize& pixelSize)
{
    wxCHECK_MSG( pixelSize.GetWidth() >= 0 && pixelSize.GetHeight() > 0, false,
                 "Negative values for the pixel size or zero pixel height are not allowed" );

    if (wx_pango_version_check(1,8,0) != NULL ||
        pixelSize.GetWidth() != 0)
    {
        // NOTE: pango_font_description_set_absolute_size() only sets the font height;
        //       if the user set the pixel width of the font explicitly or the pango
        //       library is too old, we cannot proceed
        return false;
    }

    pango_font_description_set_absolute_size( m_nativeFontInfo.description,
                                              pixelSize.GetHeight() * PANGO_SCALE );

    return true;
}
*/

void wxFontRefData::SetFamily(wxFontFamily family)
{
    m_nativeFontInfo.SetFamily(family);
}

void wxFontRefData::SetStyle(wxFontStyle style)
{
    m_nativeFontInfo.SetStyle(style);
}

void wxFontRefData::SetWeight(wxFontWeight weight)
{
    m_nativeFontInfo.SetWeight(weight);
}

void wxFontRefData::SetUnderlined(bool underlined)
{
    m_underlined = underlined;

    // the Pango font descriptor does not have an underlined attribute
    // (and wxNativeFontInfo::SetUnderlined asserts); rather it's
    // wxWindowDCImpl::DoDrawText that handles underlined fonts, so we
    // here we just need to save the underlined attribute
}

bool wxFontRefData::SetFaceName(const wxString& facename)
{
    return m_nativeFontInfo.SetFaceName(facename);
}

void wxFontRefData::SetEncoding(wxFontEncoding WXUNUSED(encoding))
{
    // with GTK+ 2 Pango always uses UTF8 internally, we cannot change it
}

void wxFontRefData::SetNativeFontInfo(const wxNativeFontInfo& info)
{
    m_nativeFontInfo = info;

    // set all the other font parameters from the native font info
    InitFromNative();
}

// ----------------------------------------------------------------------------
// wxFont creation
// ----------------------------------------------------------------------------

wxFont::wxFont(const wxNativeFontInfo& info)
{
    Create( info.GetPointSize(),
            info.GetFamily(),
            info.GetStyle(),
            info.GetWeight(),
            info.GetUnderlined(),
            info.GetFaceName(),
            info.GetEncoding() );
}

bool wxFont::Create( int pointSize,
                     wxFontFamily family,
                     wxFontStyle style,
                     wxFontWeight weight,
                     bool underlined,
                     const wxString& face,
                     wxFontEncoding encoding )
{
    UnRef();

    m_refData = new wxFontRefData(pointSize, family, style, weight,
                                  underlined, face, encoding);

    return true;
}

bool wxFont::Create(const wxString& fontname)
{
    // VZ: does this really happen?
    if ( fontname.empty() )
    {
        *this = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

        return true;
    }

    m_refData = new wxFontRefData(fontname);

    return true;
}

wxFont::~wxFont()
{
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

int wxFont::GetPointSize() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid font") );

    return M_FONTDATA->m_nativeFontInfo.GetPointSize();
}

wxString wxFont::GetFaceName() const
{
    wxCHECK_MSG( IsOk(), wxEmptyString, wxT("invalid font") );

    return M_FONTDATA->m_nativeFontInfo.GetFaceName();
}

wxFontFamily wxFont::DoGetFamily() const
{
    return M_FONTDATA->m_nativeFontInfo.GetFamily();
}

wxFontStyle wxFont::GetStyle() const
{
    wxCHECK_MSG( IsOk(), wxFONTSTYLE_MAX, wxT("invalid font") );

    return M_FONTDATA->m_nativeFontInfo.GetStyle();
}

wxFontWeight wxFont::GetWeight() const
{
    wxCHECK_MSG( IsOk(), wxFONTWEIGHT_MAX, wxT("invalid font") );

    return M_FONTDATA->m_nativeFontInfo.GetWeight();
}

bool wxFont::GetUnderlined() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid font") );

    return M_FONTDATA->m_underlined;
}

wxFontEncoding wxFont::GetEncoding() const
{
    wxCHECK_MSG( IsOk(), wxFONTENCODING_SYSTEM, wxT("invalid font") );

    return wxFONTENCODING_UTF8;
        // Pango always uses UTF8... see also SetEncoding()
}

const wxNativeFontInfo *wxFont::GetNativeFontInfo() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid font") );

    return &(M_FONTDATA->m_nativeFontInfo);
}

bool wxFont::IsFixedWidth() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid font") );

    return wxFontBase::IsFixedWidth();
}

// ----------------------------------------------------------------------------
// change font attributes
// ----------------------------------------------------------------------------

void wxFont::SetPointSize(int pointSize)
{
    AllocExclusive();

    M_FONTDATA->SetPointSize(pointSize);
}

void wxFont::SetFamily(wxFontFamily family)
{
    AllocExclusive();

    M_FONTDATA->SetFamily(family);
}

void wxFont::SetStyle(wxFontStyle style)
{
    AllocExclusive();

    M_FONTDATA->SetStyle(style);
}

void wxFont::SetWeight(wxFontWeight weight)
{
    AllocExclusive();

    M_FONTDATA->SetWeight(weight);
}

bool wxFont::SetFaceName(const wxString& faceName)
{
    AllocExclusive();

    return M_FONTDATA->SetFaceName(faceName) &&
           wxFontBase::SetFaceName(faceName);
}

void wxFont::SetUnderlined(bool underlined)
{
    AllocExclusive();

    M_FONTDATA->SetUnderlined(underlined);
}

void wxFont::SetEncoding(wxFontEncoding encoding)
{
    AllocExclusive();

    M_FONTDATA->SetEncoding(encoding);
}

void wxFont::DoSetNativeFontInfo( const wxNativeFontInfo& info )
{
    AllocExclusive();

    M_FONTDATA->SetNativeFontInfo( info );
}

wxGDIRefData* wxFont::CreateGDIRefData() const
{
    return new wxFontRefData;
}

wxGDIRefData* wxFont::CloneGDIRefData(const wxGDIRefData* data) const
{
    return new wxFontRefData(*static_cast<const wxFontRefData*>(data));
}
