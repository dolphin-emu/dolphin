/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/colour.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/colour.h"

#include <gdk/gdk.h>
#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// wxColour
//-----------------------------------------------------------------------------

class wxColourRefData : public wxGDIRefData
{
public:
#ifdef __WXGTK3__
    wxColourRefData(const GdkRGBA& gdkRGBA)
        : m_gdkRGBA(gdkRGBA)
    {
        m_gdkColor.red = guint16(gdkRGBA.red * 65535);
        m_gdkColor.green = guint16(gdkRGBA.green * 65535);
        m_gdkColor.blue = guint16(gdkRGBA.blue * 65535);
        m_alpha = wxByte(gdkRGBA.alpha * 255 + 0.5);
    }
    wxColourRefData(const GdkColor& gdkColor)
        : m_gdkColor(gdkColor)
    {
        m_gdkRGBA.red = gdkColor.red / 65535.0;
        m_gdkRGBA.green = gdkColor.green / 65535.0;
        m_gdkRGBA.blue = gdkColor.blue / 65535.0;
        m_gdkRGBA.alpha = 1;
        m_alpha = 255;
    }
    wxColourRefData(guchar red, guchar green, guchar blue, guchar alpha)
    {
        m_gdkRGBA.red = red / 255.0;
        m_gdkRGBA.green = green / 255.0;
        m_gdkRGBA.blue = blue / 255.0;
        m_gdkRGBA.alpha = alpha / 255.0;
        m_gdkColor.red = (guint16(red) << 8) + red;
        m_gdkColor.green = (guint16(green) << 8) + green;
        m_gdkColor.blue = (guint16(blue) << 8) + blue;
        m_alpha = alpha;
    }
    GdkRGBA m_gdkRGBA;
    GdkColor m_gdkColor;
#else
    wxColourRefData(guint16 red, guint16 green, guint16 blue, wxByte alpha = 0xff)
    {
        m_color.red =
        m_red = red;
        m_color.green =
        m_green = green;
        m_color.blue =
        m_blue = blue;
        m_alpha = alpha;
        m_color.pixel = 0;
        m_colormap = NULL;
    }

    virtual ~wxColourRefData()
    {
        FreeColour();
    }

    void FreeColour();
    void AllocColour( GdkColormap* cmap );

    GdkColor     m_color;
    GdkColormap *m_colormap;
    // gdk_colormap_alloc_color may change the RGB values in m_color, so we need separate copies
    guint16 m_red;
    guint16 m_green;
    guint16 m_blue;
#endif
    wxByte m_alpha;

    wxDECLARE_NO_COPY_CLASS(wxColourRefData);
};

#ifndef __WXGTK3__
void wxColourRefData::FreeColour()
{
    if (m_colormap)
    {
        gdk_colormap_free_colors(m_colormap, &m_color, 1);
        m_colormap = NULL;
        m_color.pixel = 0;
    }
}

void wxColourRefData::AllocColour( GdkColormap *cmap )
{
    if (m_colormap != cmap)
    {
        FreeColour();

        m_color.red = m_red;
        m_color.green = m_green;
        m_color.blue = m_blue;
        if (gdk_colormap_alloc_color(cmap, &m_color, FALSE, TRUE))
        {
            m_colormap = cmap;
        }
    }
}
#endif

//-----------------------------------------------------------------------------

#define M_COLDATA static_cast<wxColourRefData*>(m_refData)

// GDK's values are in 0..65535 range, ours are in 0..255
#define SHIFT  8

#ifdef __WXGTK3__
wxColour::wxColour(const GdkRGBA& gdkRGBA)
{
    m_refData = new wxColourRefData(gdkRGBA);
}

wxColour::wxColour(const GdkColor& gdkColor)
{
    m_refData = new wxColourRefData(gdkColor);
}
#else
wxColour::wxColour(const GdkColor& gdkColor)
{
    m_refData = new wxColourRefData(gdkColor.red, gdkColor.green, gdkColor.blue);
}
#endif

wxColour::~wxColour()
{
}

bool wxColour::operator == ( const wxColour& col ) const
{
    if (m_refData == col.m_refData)
        return true;

    if (!m_refData || !col.m_refData)
        return false;

    wxColourRefData* refData = M_COLDATA;
    wxColourRefData* that_refData = static_cast<wxColourRefData*>(col.m_refData);
#ifdef __WXGTK3__
    return refData->m_gdkColor.red == that_refData->m_gdkColor.red &&
           refData->m_gdkColor.green == that_refData->m_gdkColor.green &&
           refData->m_gdkColor.blue == that_refData->m_gdkColor.blue &&
#else
    return refData->m_red == that_refData->m_red &&
           refData->m_green == that_refData->m_green &&
           refData->m_blue == that_refData->m_blue &&
#endif
           refData->m_alpha == that_refData->m_alpha;
}

void wxColour::InitRGBA(unsigned char red, unsigned char green, unsigned char blue,
                        unsigned char alpha)
{
    UnRef();

#ifdef __WXGTK3__
    m_refData = new wxColourRefData(red, green, blue, alpha);
#else
    m_refData = new wxColourRefData(
        (guint16(red) << SHIFT) + red,
        (guint16(green) << SHIFT) + green,
        (guint16(blue) << SHIFT) + blue,
        alpha);
#endif
}

unsigned char wxColour::Red() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

#ifdef __WXGTK3__
    return wxByte(M_COLDATA->m_gdkColor.red >> 8);
#else
    return wxByte(M_COLDATA->m_red >> SHIFT);
#endif
}

unsigned char wxColour::Green() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

#ifdef __WXGTK3__
    return wxByte(M_COLDATA->m_gdkColor.green >> 8);
#else
    return wxByte(M_COLDATA->m_green >> SHIFT);
#endif
}

unsigned char wxColour::Blue() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

#ifdef __WXGTK3__
    return wxByte(M_COLDATA->m_gdkColor.blue >> 8);
#else
    return wxByte(M_COLDATA->m_blue >> SHIFT);
#endif
}

unsigned char wxColour::Alpha() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

    return M_COLDATA->m_alpha;
}

#ifndef __WXGTK3__
void wxColour::CalcPixel( GdkColormap *cmap )
{
    if (!IsOk()) return;

    M_COLDATA->AllocColour( cmap );
}

int wxColour::GetPixel() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

    return M_COLDATA->m_color.pixel;
}
#endif

const GdkColor *wxColour::GetColor() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid colour") );

#ifdef __WXGTK3__
    return &M_COLDATA->m_gdkColor;
#else
    return &M_COLDATA->m_color;
#endif
}

#ifdef __WXGTK3__
wxColour::operator const GdkRGBA*() const
{
    const GdkRGBA* c = NULL;
    if (IsOk())
        c = &M_COLDATA->m_gdkRGBA;
    return c;
}
#endif

bool wxColour::FromString(const wxString& str)
{
#ifdef __WXGTK3__
    GdkRGBA gdkRGBA;
    if (gdk_rgba_parse(&gdkRGBA, wxGTK_CONV_SYS(str)))
    {
        *this = wxColour(gdkRGBA);
        return true;
    }
#else
    GdkColor colGDK;
    if ( gdk_color_parse( wxGTK_CONV_SYS( str ), &colGDK ) )
    {
        *this = wxColour(colGDK);
        return true;
    }
#endif

    return wxColourBase::FromString(str);
}
