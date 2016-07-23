/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/font.cpp
// Purpose:     wxFont class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/font.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/gdicmn.h"
    #include "wx/log.h"
    #include "wx/math.h"
#endif

#include "wx/fontutil.h"
#include "wx/graphics.h"
#include "wx/settings.h"
#include "wx/tokenzr.h"

#include "wx/osx/private.h"

#include <map>
#include <string>

class WXDLLEXPORT wxFontRefData: public wxGDIRefData
{
public:

    wxFontRefData()
    {
        Init();
        m_info.Init(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
             false, false, wxEmptyString, wxFONTENCODING_DEFAULT);
    }

    wxFontRefData(const wxFontRefData& data);

    wxFontRefData( const wxNativeFontInfo& info ) : m_info(info)
    {
        Init();
    }

    wxFontRefData(wxOSXSystemFont font, int size);

    wxFontRefData( wxUint32 coreTextFontType );
    wxFontRefData( CTFontRef font );
    wxFontRefData( CTFontDescriptorRef fontdescriptor, int size );

    virtual ~wxFontRefData();

    void SetPointSize( int size )
    {
        if( GetPointSize() != size )
        {
            m_info.SetPointSize(size);
            Free();
        }
    }

    int GetPointSize() const { return m_info.GetPointSize(); }

    void SetFamily( wxFontFamily family )
    {
        if ( m_info.m_family != family )
        {
            m_info.SetFamily( family );
            Free();
        }
    }

    wxFontFamily GetFamily() const { return m_info.GetFamily(); }

    void SetStyle( wxFontStyle style )
    {
        if ( m_info.m_style != style )
        {
            m_info.SetStyle( style );
            Free();
        }
    }


    wxFontStyle GetStyle() const { return m_info.GetStyle(); }

    void SetWeight( wxFontWeight weight )
    {
        if ( m_info.m_weight != weight )
        {
            m_info.SetWeight( weight );
            Free();
        }
    }


    wxFontWeight GetWeight() const { return m_info.GetWeight(); }

    void SetStrikethrough( bool s )
    {
        if ( m_info.m_strikethrough != s )
        {
            m_info.SetStrikethrough( s );
            Free();
        }
    }

    void SetUnderlined( bool u )
    {
        if ( m_info.m_underlined != u )
        {
            m_info.SetUnderlined( u );
            Free();
        }
    }

    bool GetUnderlined() const { return m_info.GetUnderlined(); }
    bool GetStrikethrough() const { return m_info.GetStrikethrough(); }

    void SetFaceName( const wxString& facename )
    {
        if ( m_info.m_faceName != facename )
        {
            m_info.SetFaceName( facename );
            Free();
        }
    }

    wxString GetFaceName() const { return m_info.GetFaceName(); }

    void SetEncoding( wxFontEncoding encoding )
    {
        if ( m_info.m_encoding != encoding )
        {
            m_info.SetEncoding( encoding );
            Free();
        }
    }

    wxFontEncoding GetEncoding() const { return m_info.GetEncoding(); }
    
    bool IsFixedWidth() const;

    void Free();

    void MacFindFont();

protected:
    // common part of all ctors
    void Init();
public:
    bool            m_fontValid;
    wxCFRef<CTFontRef> m_ctFont;
#if wxOSX_USE_ATSU_TEXT
    void CreateATSUFont();

    ATSUStyle       m_macATSUStyle ;
#endif
    wxCFRef<CGFontRef> m_cgFont;
#if wxOSX_USE_COCOA
    WX_NSFont       m_nsFont;
#endif
#if wxOSX_USE_IPHONE
    WX_UIFont       m_uiFont;
#endif
    wxNativeFontInfo  m_info;
};

#define M_FONTDATA ((wxFontRefData*)m_refData)

wxFontRefData::wxFontRefData(const wxFontRefData& data) : wxGDIRefData()
{
    Init();
    m_info = data.m_info;
    m_fontValid = data.m_fontValid;
    m_ctFont = data.m_ctFont;
    m_cgFont = data.m_cgFont;
#if wxOSX_USE_ATSU_TEXT
    if ( data.m_macATSUStyle != NULL )
    {
        ATSUCreateStyle(&m_macATSUStyle) ;
        ATSUCopyAttributes(data.m_macATSUStyle, m_macATSUStyle);
    }
#endif
#if wxOSX_USE_COCOA
    m_nsFont = (NSFont*) wxMacCocoaRetain(data.m_nsFont);
#endif
#if wxOSX_USE_IPHONE
    m_uiFont = (UIFont*) wxMacCocoaRetain(data.m_uiFont);
#endif

}

// ============================================================================
// implementation
// ============================================================================

wxStringToStringHashMap gs_FontFamilyToPSName;
static CTFontDescriptorRef wxMacCreateCTFontDescriptor(CFStringRef iFamilyName, CTFontSymbolicTraits iTraits );

// ----------------------------------------------------------------------------
// wxFontRefData
// ----------------------------------------------------------------------------

void wxFontRefData::Init()
{
#if wxOSX_USE_ATSU_TEXT
    m_macATSUStyle = NULL ;
#endif
#if wxOSX_USE_COCOA
    m_nsFont = NULL;
#endif
#if wxOSX_USE_IPHONE
    m_uiFont = NULL;
#endif
    m_fontValid = false;
}

wxFontRefData::~wxFontRefData()
{
    Free();
}

void wxFontRefData::Free()
{
    m_ctFont.reset();
    m_cgFont.reset();
#if wxOSX_USE_ATSU_TEXT
    if ( m_macATSUStyle )
    {
        ::ATSUDisposeStyle((ATSUStyle)m_macATSUStyle);
        m_macATSUStyle = NULL ;
    }
#endif
#if wxOSX_USE_COCOA
    if (m_nsFont != NULL)
    {
        wxMacCocoaRelease(m_nsFont);
        m_nsFont = NULL;
    }
#endif
#if wxOSX_USE_IPHONE
    if (m_uiFont != NULL)
    {
        wxMacCocoaRelease(m_uiFont);
        m_uiFont = NULL;
    }
#endif
    m_fontValid = false;
}

wxFontRefData::wxFontRefData(wxOSXSystemFont font, int size)
{
    wxASSERT( font != wxOSX_SYSTEM_FONT_NONE );
    Init();

    {
        CTFontUIFontType uifont = kCTFontSystemFontType;
        switch( font )
        {
            case wxOSX_SYSTEM_FONT_NORMAL:
                uifont = kCTFontSystemFontType;
                break;
            case wxOSX_SYSTEM_FONT_BOLD:
                uifont = kCTFontEmphasizedSystemFontType;
                break;
            case wxOSX_SYSTEM_FONT_SMALL:
                uifont = kCTFontSmallSystemFontType;
                break;
            case wxOSX_SYSTEM_FONT_SMALL_BOLD:
                uifont = kCTFontSmallEmphasizedSystemFontType;
                break;
            case wxOSX_SYSTEM_FONT_MINI:
                uifont = kCTFontMiniSystemFontType;
                break;
           case wxOSX_SYSTEM_FONT_MINI_BOLD:
                uifont = kCTFontMiniEmphasizedSystemFontType;
                break;
            case wxOSX_SYSTEM_FONT_LABELS:
                uifont = kCTFontLabelFontType;
                break;
           case wxOSX_SYSTEM_FONT_VIEWS:
                uifont = kCTFontViewsFontType;
                break;
            default:
                break;
        }
        m_ctFont.reset(CTFontCreateUIFontForLanguage( uifont, (CGFloat) size, NULL ));
        wxCFRef<CTFontDescriptorRef> descr;
        descr.reset( CTFontCopyFontDescriptor( m_ctFont ) );
        m_info.Init(descr);
    }
#if wxOSX_USE_ATSU_TEXT
    {
        // not needed outside
        ThemeFontID m_macThemeFontID = kThemeSystemFont;
        switch( font )
        {
            case wxOSX_SYSTEM_FONT_NORMAL:
                m_macThemeFontID = kThemeSystemFont;
                break;
            case wxOSX_SYSTEM_FONT_BOLD:
                m_macThemeFontID = kThemeEmphasizedSystemFont;
                break;
            case wxOSX_SYSTEM_FONT_SMALL:
                m_macThemeFontID = kThemeSmallSystemFont;
                break;
            case wxOSX_SYSTEM_FONT_SMALL_BOLD:
                m_macThemeFontID = kThemeSmallEmphasizedSystemFont;
                break;
            case wxOSX_SYSTEM_FONT_MINI:
                m_macThemeFontID = kThemeMiniSystemFont;
                break;
           case wxOSX_SYSTEM_FONT_MINI_BOLD:
                // bold not available under theming
                m_macThemeFontID = kThemeMiniSystemFont;
                break;
            case wxOSX_SYSTEM_FONT_LABELS:
                m_macThemeFontID = kThemeLabelFont;
                break;
           case wxOSX_SYSTEM_FONT_VIEWS:
                m_macThemeFontID = kThemeViewsFont;
                break;
            default:
                break;
        }
        if ( m_info.m_faceName.empty() )
        {
            Style style ;
            FMFontSize fontSize;
            Str255 qdFontName ;

            GetThemeFont( m_macThemeFontID, GetApplicationScript(), qdFontName, &fontSize, &style );
            if ( size != 0 )
                fontSize = size;

            wxFontStyle fontstyle = wxFONTSTYLE_NORMAL;
            wxFontWeight fontweight = wxFONTWEIGHT_NORMAL;
            bool underlined = false;
            bool strikethrough = false;

            if ( style & bold )
                fontweight = wxFONTWEIGHT_BOLD ;
            else
                fontweight = wxFONTWEIGHT_NORMAL ;
            if ( style & italic )
                fontstyle = wxFONTSTYLE_ITALIC ;
            if ( style & underline )
                underlined = true ;

            m_info.Init(fontSize,wxFONTFAMILY_DEFAULT,fontstyle,
                fontweight,underlined, strikethrough,
                wxMacMakeStringFromPascal( qdFontName ), wxFONTENCODING_DEFAULT);
         }
    }
#endif
#if wxOSX_USE_COCOA
    m_nsFont = wxFont::OSXCreateNSFont( font, &m_info );
#endif
#if wxOSX_USE_IPHONE
    m_uiFont = wxFont::OSXCreateUIFont( font, &m_info );
#endif
    m_info.EnsureValid();
#if wxOSX_USE_ATSU_TEXT
    CreateATSUFont();
#endif

    m_fontValid = true;
}

#if wxOSX_USE_ATSU_TEXT
void wxFontRefData::CreateATSUFont()
{
    // we try to get as much styles as possible into ATSU

    OSStatus status = ::ATSUCreateStyle(&m_macATSUStyle);
    wxASSERT_MSG( status == noErr , wxT("couldn't create ATSU style") );

    ATSUAttributeTag atsuTags[] =
    {
        kATSUFontTag ,
        kATSUSizeTag ,
        kATSUVerticalCharacterTag,
        kATSUQDBoldfaceTag ,
        kATSUQDItalicTag ,
        kATSUQDUnderlineTag ,
        kATSUQDCondensedTag ,
        kATSUQDExtendedTag ,
        kATSUStyleStrikeThroughTag
    };
    ByteCount atsuSizes[WXSIZEOF(atsuTags)] =
    {
        sizeof( ATSUFontID ) ,
        sizeof( Fixed ) ,
        sizeof( ATSUVerticalCharacterType),
        sizeof( Boolean ) ,
        sizeof( Boolean ) ,
        sizeof( Boolean ) ,
        sizeof( Boolean ) ,
        sizeof( Boolean ) ,
        sizeof( Boolean )
    };

    Boolean kTrue = true ;
    Boolean kFalse = false ;

    Fixed atsuSize = IntToFixed( m_info.m_pointSize );
    ATSUVerticalCharacterType kHorizontal = kATSUStronglyHorizontal;
    FMFontStyle addQDStyle = m_info.m_atsuAdditionalQDStyles;
    ATSUAttributeValuePtr    atsuValues[WXSIZEOF(atsuTags)] =
    {
        &m_info.m_atsuFontID ,
        &atsuSize ,
        &kHorizontal,
        (addQDStyle & bold) ? &kTrue : &kFalse ,
        (addQDStyle & italic) ? &kTrue : &kFalse ,
        (addQDStyle & underline) ? &kTrue : &kFalse ,
        (addQDStyle & condense) ? &kTrue : &kFalse ,
        (addQDStyle & extend) ? &kTrue : &kFalse ,
        m_info.m_strikethrough ? & kTrue : &kFalse
    };

    status = ::ATSUSetAttributes(
                                 (ATSUStyle)m_macATSUStyle,
                                 WXSIZEOF(atsuTags),
                                 atsuTags, atsuSizes, atsuValues);

    wxASSERT_MSG( status == noErr , wxString::Format(wxT("couldn't modify ATSU style. Status was %d"), (int) status).c_str() );

    if ( m_cgFont.get() == NULL )
    {
        ATSFontRef fontRef = FMGetATSFontRefFromFont(m_info.m_atsuFontID);
        m_cgFont.reset( CGFontCreateWithPlatformFont( &fontRef ) );
    }
}
#endif

static const CGAffineTransform kSlantTransform = CGAffineTransformMake( 1, 0, tan(wxDegToRad(11)), 1, 0, 0 );

void wxFontRefData::MacFindFont()
{
    if ( m_fontValid )
        return;

    wxCHECK_RET( m_info.m_pointSize > 0, wxT("Point size should not be zero.") );

    m_info.EnsureValid();

    {
         CTFontSymbolicTraits traits = 0;

        if (m_info.m_weight == wxFONTWEIGHT_BOLD)
            traits |= kCTFontBoldTrait;
        if (m_info.m_style == wxFONTSTYLE_ITALIC || m_info.m_style == wxFONTSTYLE_SLANT)
            traits |= kCTFontItalicTrait;

        // use font caching
        wxString lookupnameWithSize = wxString::Format( "%s_%u_%d", m_info.m_faceName, traits, m_info.m_pointSize );

        static std::map< std::wstring , wxCFRef< CTFontRef > > fontcache ;
        m_ctFont = fontcache[ std::wstring(lookupnameWithSize.wc_str()) ];
        if ( !m_ctFont )
        {

            wxStringToStringHashMap::const_iterator it = gs_FontFamilyToPSName.find(m_info.m_faceName);

            if ( it != gs_FontFamilyToPSName.end() )
            {
                 m_ctFont.reset(CTFontCreateWithName( wxCFStringRef(it->second), m_info.m_pointSize , NULL ));
            }
            else
            {
                wxCFRef<CTFontDescriptorRef> desc(wxMacCreateCTFontDescriptor(wxCFStringRef(m_info.m_faceName),0));
                m_ctFont.reset(CTFontCreateWithFontDescriptor(desc, m_info.m_pointSize , NULL ));
                m_info.UpdateNamesMap(m_info.m_faceName, m_ctFont);
           }

            if ( m_ctFont.get() == NULL )
            {
                // TODO try fallbacks according to font type
                m_ctFont.reset(CTFontCreateUIFontForLanguage( kCTFontSystemFontType, m_info.m_pointSize , NULL ));
            }
            else
            {
                if ( traits != 0 )
                {
                    // attempt native font variant, if not available, fallback to italic emulation mode and remove bold
                    CTFontRef fontWithTraits = CTFontCreateCopyWithSymbolicTraits( m_ctFont, 0, NULL, traits, traits );
                    if ( fontWithTraits == NULL )
                    {
                        CTFontSymbolicTraits remainingTraits = traits;
                        const CGAffineTransform* remainingTransform = NULL;

                        if( remainingTraits & kCTFontItalicTrait )
                        {
                            remainingTraits &= ~kCTFontItalicTrait;
                            remainingTransform = &kSlantTransform;
                            if ( remainingTraits & kCTFontBoldTrait )
                            {
                                // first try an emulated oblique with an existing bold font
                                fontWithTraits = CTFontCreateCopyWithSymbolicTraits( m_ctFont, 0, remainingTransform, remainingTraits, remainingTraits );
                                if ( fontWithTraits == NULL )
                                {
                                    // give in on the bold, try native oblique
                                    fontWithTraits = CTFontCreateCopyWithSymbolicTraits( m_ctFont, 0, NULL, kCTFontItalicTrait, kCTFontItalicTrait );
                                }
                            }
                        }

                        if ( fontWithTraits == NULL )
                        {
                            fontWithTraits = CTFontCreateCopyWithAttributes( m_ctFont, m_info.m_pointSize, remainingTransform, NULL );
                        }

                    }
                    if ( fontWithTraits != NULL )
                        m_ctFont.reset(fontWithTraits);
                }
            }
        }

        m_cgFont.reset(CTFontCopyGraphicsFont(m_ctFont, NULL));
    }
#if wxOSX_USE_ATSU_TEXT
    CreateATSUFont();
#endif
#if wxOSX_USE_COCOA
    m_nsFont = wxFont::OSXCreateNSFont( &m_info );
#endif
#if wxOSX_USE_IPHONE
    m_uiFont = wxFont::OSXCreateUIFont( &m_info );
#endif
    m_fontValid = true;
}

bool wxFontRefData::IsFixedWidth() const
{
    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(m_ctFont);
    return (traits & kCTFontMonoSpaceTrait) != 0;
}

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

bool wxFont::Create(const wxNativeFontInfo& info)
{
    UnRef();

    m_refData = new wxFontRefData( info );
    RealizeResource();

    return true;
}

wxFont::wxFont(wxOSXSystemFont font)
{
    m_refData = new wxFontRefData( font, 0 );
}

wxFont::wxFont(const wxString& fontdesc)
{
    wxNativeFontInfo info;
    if ( info.FromString(fontdesc) )
        (void)Create(info);
}

wxFont::wxFont(int size,
               int family,
               int style,
               int weight,
               bool underlined,
               const wxString& face,
               wxFontEncoding encoding)
{
    (void)Create(size, (wxFontFamily)family, (wxFontStyle)style,
                (wxFontWeight)weight, underlined, face, encoding);
}

bool wxFont::Create(int pointSize,
                    wxFontFamily family,
                    wxFontStyle style,
                    wxFontWeight weight,
                    bool underlined,
                    const wxString& faceNameParam,
                    wxFontEncoding encoding)
{
    UnRef();

    wxString faceName = faceNameParam;

    if ( faceName.empty() )
    {
        switch ( family )
        {
            case wxFONTFAMILY_DEFAULT :
                faceName = wxT("Lucida Grande");
                break;

            case wxFONTFAMILY_SCRIPT :
            case wxFONTFAMILY_ROMAN :
            case wxFONTFAMILY_DECORATIVE :
                faceName = wxT("Times");
                break ;

            case wxFONTFAMILY_SWISS :
                faceName =  wxT("Helvetica");
                break ;

            case wxFONTFAMILY_MODERN :
            case wxFONTFAMILY_TELETYPE:
                faceName =  wxT("Courier");
                 break ;

            default:
                faceName =  wxT("Times");
                break ;
        }
    }

    wxNativeFontInfo info;

    info.Init(pointSize, family, style, weight,
        underlined, false, faceName, encoding);

    m_refData = new wxFontRefData(info);

    return true;
}

wxFont::~wxFont()
{
}

void wxFont::DoSetNativeFontInfo(const wxNativeFontInfo& info)
{
    UnRef();

    m_refData = new wxFontRefData( info);
}


bool wxFont::RealizeResource()
{
    M_FONTDATA->MacFindFont();

    return true;
}

void wxFont::SetEncoding(wxFontEncoding encoding)
{
    AllocExclusive();

    M_FONTDATA->SetEncoding( encoding );
}

wxGDIRefData *wxFont::CreateGDIRefData() const
{
    return new wxFontRefData;
}

wxGDIRefData *wxFont::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxFontRefData(*static_cast<const wxFontRefData *>(data));
}

void wxFont::SetPointSize(int pointSize)
{
    if ( M_FONTDATA != NULL && M_FONTDATA->GetPointSize() == pointSize )
        return;

    AllocExclusive();

    M_FONTDATA->SetPointSize( pointSize );
}

void wxFont::SetFamily(wxFontFamily family)
{
    AllocExclusive();

    M_FONTDATA->SetFamily( family );
}

void wxFont::SetStyle(wxFontStyle style)
{
    AllocExclusive();

    M_FONTDATA->SetStyle( style );
}

void wxFont::SetWeight(wxFontWeight weight)
{
    AllocExclusive();

    M_FONTDATA->SetWeight( weight );
}

bool wxFont::SetFaceName(const wxString& faceName)
{
    AllocExclusive();

    M_FONTDATA->SetFaceName( faceName );

    return wxFontBase::SetFaceName(faceName);
}

void wxFont::SetUnderlined(bool underlined)
{
    AllocExclusive();

    M_FONTDATA->SetUnderlined( underlined );
}

void wxFont::SetStrikethrough(bool strikethrough)
{
     AllocExclusive();

     M_FONTDATA->SetStrikethrough( strikethrough );
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

// TODO: insert checks everywhere for M_FONTDATA == NULL!

int wxFont::GetPointSize() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , 0, wxT("invalid font") );

    return M_FONTDATA->GetPointSize();
}

wxSize wxFont::GetPixelSize() const
{
#if wxUSE_GRAPHICS_CONTEXT
    // TODO: consider caching the value
    wxGraphicsContext* dc = wxGraphicsContext::CreateFromNative((CGContextRef) NULL);
    dc->SetFont(*(wxFont *)this,*wxBLACK);
    wxDouble width, height = 0;
    dc->GetTextExtent( wxT("g"), &width, &height, NULL, NULL);
    delete dc;
    return wxSize((int)width, (int)height);
#else
    return wxFontBase::GetPixelSize();
#endif
}

bool wxFont::IsFixedWidth() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , false, wxT("invalid font") );
    
    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return M_FONTDATA->IsFixedWidth();
}

wxFontFamily wxFont::DoGetFamily() const
{
    return M_FONTDATA->GetFamily();
}

wxFontStyle wxFont::GetStyle() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , wxFONTSTYLE_MAX, wxT("invalid font") );

    return M_FONTDATA->GetStyle() ;
}

wxFontWeight wxFont::GetWeight() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , wxFONTWEIGHT_MAX, wxT("invalid font") );

    return M_FONTDATA->GetWeight();
}

bool wxFont::GetUnderlined() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , false, wxT("invalid font") );

    return M_FONTDATA->GetUnderlined();
}

bool wxFont::GetStrikethrough() const
{
    wxCHECK_MSG( M_FONTDATA != NULL, false, wxT("invalid font") );

    return M_FONTDATA->GetStrikethrough();
}

wxString wxFont::GetFaceName() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , wxEmptyString , wxT("invalid font") );

    return M_FONTDATA->GetFaceName() ;
}

wxFontEncoding wxFont::GetEncoding() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , wxFONTENCODING_DEFAULT , wxT("invalid font") );

    return M_FONTDATA->GetEncoding() ;
}

#if wxOSX_USE_ATSU_TEXT
void * wxFont::MacGetATSUStyle() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , NULL, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return M_FONTDATA->m_macATSUStyle;
}

wxUint32 wxFont::MacGetATSUFontID() const
{
    wxCHECK_MSG( M_FONTDATA != NULL, 0, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return M_FONTDATA->m_info.m_atsuFontID;
}

wxUint32 wxFont::MacGetATSUAdditionalQDStyles() const
{
    wxCHECK_MSG( M_FONTDATA != NULL, 0, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return M_FONTDATA->m_info.m_atsuAdditionalQDStyles;
}
#endif

CTFontRef wxFont::OSXGetCTFont() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , 0, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return (CTFontRef)(M_FONTDATA->m_ctFont);
}

#if wxOSX_USE_COCOA_OR_CARBON

CGFontRef wxFont::OSXGetCGFont() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , 0, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return (M_FONTDATA->m_cgFont);
}

#endif


#if wxOSX_USE_COCOA

NSFont* wxFont::OSXGetNSFont() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , 0, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return (M_FONTDATA->m_nsFont);
}

#endif

#if wxOSX_USE_IPHONE

UIFont* wxFont::OSXGetUIFont() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , 0, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    return (M_FONTDATA->m_uiFont);
}

#endif

const wxNativeFontInfo * wxFont::GetNativeFontInfo() const
{
    wxCHECK_MSG( M_FONTDATA != NULL , NULL, wxT("invalid font") );
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid font") );

    // cast away constness otherwise lazy font resolution is not possible
    const_cast<wxFont *>(this)->RealizeResource();

    // M_FONTDATA->m_info.InitFromFont(*this);

    return &(M_FONTDATA->m_info);
}

// ----------------------------------------------------------------------------
// wxNativeFontInfo
// ----------------------------------------------------------------------------

/* from Core Text Manual Common Operations */

static CTFontDescriptorRef wxMacCreateCTFontDescriptor(CFStringRef iFamilyName, CTFontSymbolicTraits iTraits )
{
    CTFontDescriptorRef descriptor = NULL;
    CFMutableDictionaryRef attributes;

    wxASSERT(iFamilyName != NULL);
    // Create a mutable dictionary to hold our attributes.
    attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    wxASSERT(attributes != NULL);

    if (attributes != NULL) {
        // Add a family name to our attributes.
        CFDictionaryAddValue(attributes, kCTFontFamilyNameAttribute, iFamilyName);


        if ( iTraits ) {
            CFMutableDictionaryRef traits;
            CFNumberRef symTraits;

            // Create the traits dictionary.
            symTraits = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                                       &iTraits);
            wxASSERT(symTraits != NULL);

            if (symTraits != NULL) {
                // Create a dictionary to hold our traits values.
                traits = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                wxASSERT(traits != NULL);

                if (traits != NULL) {
                    // Add the symbolic traits value to the traits dictionary.
                    CFDictionaryAddValue(traits, kCTFontSymbolicTrait, symTraits);

                    // Add the traits attribute to our attributes.
                    CFDictionaryAddValue(attributes, kCTFontTraitsAttribute, traits);
                    CFRelease(traits);
                }
                CFRelease(symTraits);
            }
        }
        // Create the font descriptor with our attributes
        descriptor = CTFontDescriptorCreateWithAttributes(attributes);
        wxASSERT(descriptor != NULL);

        CFRelease(attributes);
    }
    // Return our font descriptor.
    return descriptor ;
}


void wxNativeFontInfo::Init()
{
#if wxOSX_USE_ATSU_TEXT
    m_atsuFontID = 0 ;
    m_atsuAdditionalQDStyles = 0;
    m_atsuFontValid = false;
#endif
    m_pointSize = 0;
    m_family = wxFONTFAMILY_DEFAULT;
    m_style = wxFONTSTYLE_NORMAL;
    m_weight = wxFONTWEIGHT_NORMAL;
    m_underlined = false;
    m_strikethrough = false;
    m_faceName.clear();
    m_encoding = wxFont::GetDefaultEncoding();
    m_descriptorValid = false;
}

void wxNativeFontInfo::Init(CTFontDescriptorRef descr)
{
    Init();

    wxCFRef< CFNumberRef > sizevalue( (CFNumberRef) CTFontDescriptorCopyAttribute( descr, kCTFontSizeAttribute ) );
    float fsize;
    if ( CFNumberGetValue( sizevalue , kCFNumberFloatType , &fsize ) )
        m_pointSize = (int)( fsize + 0.5 );

    wxCFRef< CFDictionaryRef > traitsvalue( (CFDictionaryRef) CTFontDescriptorCopyAttribute( descr, kCTFontTraitsAttribute ) );
    CTFontSymbolicTraits traits;
    if ( CFNumberGetValue((CFNumberRef) CFDictionaryGetValue(traitsvalue,kCTFontSymbolicTrait),kCFNumberIntType,&traits) )
    {
        if ( traits & kCTFontItalicTrait )
            m_style = wxFONTSTYLE_ITALIC;
        if (  traits & kCTFontBoldTrait )
            m_weight = wxFONTWEIGHT_BOLD ;
    }

    wxCFStringRef familyName( (CFStringRef) CTFontDescriptorCopyAttribute(descr, kCTFontFamilyNameAttribute));
    m_faceName = familyName.AsString();

    UpdateNamesMap(m_faceName, descr);
}

void wxNativeFontInfo::EnsureValid()
{
    if ( m_descriptorValid )
        return;

#if wxOSX_USE_ATSU_TEXT
    if ( !m_atsuFontValid )
    {
        // not needed outside
        wxInt16 m_qdFontFamily;
        wxInt16 m_qdFontStyle;
        wxCFStringRef cf( m_faceName, wxLocale::GetSystemEncoding() );
        ATSFontFamilyRef atsfamily = ATSFontFamilyFindFromName( cf , kATSOptionFlagsDefault );
        if ( atsfamily == (ATSFontFamilyRef) -1 )
        {
            wxLogDebug( wxT("ATSFontFamilyFindFromName failed for ") + m_faceName );
            m_qdFontFamily = GetAppFont();
        }
        else
        {
            m_qdFontFamily = FMGetFontFamilyFromATSFontFamilyRef( atsfamily );
        }

        m_qdFontStyle = 0;
        if (m_weight == wxFONTWEIGHT_BOLD)
            m_qdFontStyle |= bold;
        if (m_style == wxFONTSTYLE_ITALIC || m_style == wxFONTSTYLE_SLANT)
            m_qdFontStyle |= italic;
        if (m_underlined)
            m_qdFontStyle |= underline;


        // we try to get as much styles as possible into ATSU

        // ATSUFontID and FMFont are equivalent
        FMFontStyle intrinsicStyle = 0 ;
        OSStatus status = FMGetFontFromFontFamilyInstance( m_qdFontFamily , m_qdFontStyle , (FMFont*)&m_atsuFontID , &intrinsicStyle);
        if ( status != noErr )
        {
            wxFAIL_MSG( wxT("couldn't get an ATSUFont from font family") );
        }
        m_atsuAdditionalQDStyles = m_qdFontStyle & (~intrinsicStyle );
        m_atsuFontValid = true;
    }
#endif
    m_descriptorValid = true;
}

void wxNativeFontInfo::Init(const wxNativeFontInfo& info)
{
    Init();
#if wxOSX_USE_ATSU_TEXT
    m_atsuFontValid = info.m_atsuFontValid;
    m_atsuFontID = info.m_atsuFontID ;
    m_atsuAdditionalQDStyles = info.m_atsuAdditionalQDStyles;
#endif
    m_pointSize = info.m_pointSize;
    m_family = info.m_family;
    m_style = info.m_style;
    m_weight = info.m_weight;
    m_underlined = info.m_underlined;
    m_strikethrough = info.m_strikethrough;
    m_faceName = info.m_faceName;
    m_encoding = info.m_encoding;
    m_descriptorValid = info.m_descriptorValid;
}

void wxNativeFontInfo::Init(int size,
                  wxFontFamily family,
                  wxFontStyle style,
                  wxFontWeight weight,
                  bool underlined,
                  bool strikethrough,
                  const wxString& faceName,
                  wxFontEncoding encoding)
{
    Init();

    // We should use the default font size if the special value wxDEFAULT is
    // specified and we also handle -1 as a synonym for wxDEFAULT for
    // compatibility with wxGTK (see #12541).
    //
    // Notice that we rely on the fact that wxNORMAL_FONT itself is not
    // initialized using this ctor, but from native font info.
    m_pointSize = size == -1 || size == wxDEFAULT
                    ? wxNORMAL_FONT->GetPointSize()
                    : size;
    m_family = family;
    m_style = style;
    m_weight = weight;
    m_underlined = underlined;
    m_strikethrough = strikethrough;
    m_faceName = faceName;
    if ( encoding == wxFONTENCODING_DEFAULT )
        encoding = wxFont::GetDefaultEncoding();
    m_encoding = encoding;

}

void wxNativeFontInfo::Free()
{
#if wxOSX_USE_ATSU_TEXT
    m_atsuFontID = 0 ;
    m_atsuAdditionalQDStyles = 0;
    m_atsuFontValid = false;
#endif
    m_descriptorValid = false;
}

bool wxNativeFontInfo::FromString(const wxString& s)
{
    long l, version;

    wxStringTokenizer tokenizer(s, wxT(";"));

    wxString token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    version = l;
    //
    //  Ignore the version for now
    //

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    m_pointSize = (int)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    m_family = (wxFontFamily)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    m_style = (wxFontStyle)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    m_weight = (wxFontWeight)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    m_underlined = l != 0;

    if ( version == 0L )
    {
        m_strikethrough = false;
    }
    else
    {
        token = tokenizer.GetNextToken();
        if ( !token.ToLong(&l) )
            return false;
        m_strikethrough = l != 0;
    }

    m_faceName = tokenizer.GetNextToken();

#ifndef __WXMAC__
    if( !faceName )
        return false;
#endif

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    m_encoding = (wxFontEncoding)l;

    return true;
}

wxString wxNativeFontInfo::ToString() const
{
    wxString s;

    s.Printf(wxT("%d;%d;%d;%d;%d;%d;%d;%s;%d"),
             1,                                 // version
             m_pointSize,
             m_family,
             (int)m_style,
             (int)m_weight,
             m_underlined,
             m_strikethrough,
             m_faceName.GetData(),
             (int)m_encoding);

    return s;
}

int wxNativeFontInfo::GetPointSize() const
{
    return m_pointSize;
}

wxFontStyle wxNativeFontInfo::GetStyle() const
{
    return m_style;
}

wxFontWeight wxNativeFontInfo::GetWeight() const
{
    return m_weight;
}

bool wxNativeFontInfo::GetUnderlined() const
{
    return m_underlined;
}

wxString wxNativeFontInfo::GetFaceName() const
{
    return m_faceName;
}

wxFontFamily wxNativeFontInfo::GetFamily() const
{
    return m_family;
}

wxFontEncoding wxNativeFontInfo::GetEncoding() const
{
    return m_encoding;
}

bool wxNativeFontInfo::GetStrikethrough() const
{
    return m_strikethrough;
}


// changing the font descriptor

void wxNativeFontInfo::SetPointSize(int pointsize)
{
    if ( m_pointSize != pointsize )
    {
        m_pointSize = pointsize;
        Free();
    }
}

void wxNativeFontInfo::SetStyle(wxFontStyle style_)
{
    if ( m_style != style_ )
    {
        m_style = style_;
        Free();
    }
}

void wxNativeFontInfo::SetWeight(wxFontWeight weight_)
{
    if ( m_weight != weight_ )
    {
        m_weight = weight_;
        Free();
    }
}

void wxNativeFontInfo::SetUnderlined(bool underlined_)
{
    if ( m_underlined != underlined_ )
    {
        m_underlined = underlined_;
        Free();
    }
}

bool wxNativeFontInfo::SetFaceName(const wxString& facename_)
{
    if ( m_faceName != facename_ )
    {
        m_faceName = facename_;
        Free();
    }
    return true;
}

void wxNativeFontInfo::SetFamily(wxFontFamily family_)
{
    if ( m_family != family_ )
    {
        m_family = family_;
        Free();
    }
}

void wxNativeFontInfo::SetEncoding(wxFontEncoding encoding_)
{
    if ( encoding_ == wxFONTENCODING_DEFAULT )
        encoding_ = wxFont::GetDefaultEncoding();
    m_encoding = encoding_;
    // not reflected in native descriptors
}

void wxNativeFontInfo::SetStrikethrough(bool strikethrough)
{
    m_strikethrough = strikethrough;
}

void wxNativeFontInfo::UpdateNamesMap(const wxString& familyName, CTFontDescriptorRef descr)
{
    if ( gs_FontFamilyToPSName.find(familyName) == gs_FontFamilyToPSName.end() )
    {
        wxCFStringRef psName( (CFStringRef) CTFontDescriptorCopyAttribute(descr, kCTFontNameAttribute));
        gs_FontFamilyToPSName[familyName] = psName.AsString();
    }
}

void wxNativeFontInfo::UpdateNamesMap(const wxString& familyName, CTFontRef font)
{
    if ( gs_FontFamilyToPSName.find(familyName) == gs_FontFamilyToPSName.end() )
    {
        wxCFRef<CTFontDescriptorRef> descr(CTFontCopyFontDescriptor( font ));
        UpdateNamesMap(familyName, descr);
    }
}

