/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/colour.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: colour.cpp 67681 2011-05-03 16:29:04Z DS $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/colour.h"

#include "wx/gtk/private.h"

#include <gdk/gdk.h>

//-----------------------------------------------------------------------------
// wxColour
//-----------------------------------------------------------------------------

class wxColourRefData : public wxGDIRefData
{
public:
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
    wxByte m_alpha;

    wxDECLARE_NO_COPY_CLASS(wxColourRefData);
};

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

//-----------------------------------------------------------------------------

#define M_COLDATA static_cast<wxColourRefData*>(m_refData)

// GDK's values are in 0..65535 range, ours are in 0..255
#define SHIFT  8

wxColour::wxColour(const GdkColor& gdkColor)
{
    m_refData = new wxColourRefData(gdkColor.red, gdkColor.green, gdkColor.blue);
}

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
    return refData->m_red == that_refData->m_red &&
           refData->m_green == that_refData->m_green &&
           refData->m_blue == that_refData->m_blue &&
           refData->m_alpha == that_refData->m_alpha;
}

void wxColour::InitRGBA(unsigned char red, unsigned char green, unsigned char blue,
                        unsigned char alpha)
{
    UnRef();

    m_refData = new wxColourRefData(
        (guint16(red) << SHIFT) + red,
        (guint16(green) << SHIFT) + green,
        (guint16(blue) << SHIFT) + blue,
        alpha);
}

unsigned char wxColour::Red() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

    return wxByte(M_COLDATA->m_red >> SHIFT);
}

unsigned char wxColour::Green() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

    return wxByte(M_COLDATA->m_green >> SHIFT);
}

unsigned char wxColour::Blue() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

    return wxByte(M_COLDATA->m_blue >> SHIFT);
}

unsigned char wxColour::Alpha() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid colour") );

    return M_COLDATA->m_alpha;
}

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

const GdkColor *wxColour::GetColor() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid colour") );

    return &M_COLDATA->m_color;
}

bool wxColour::FromString(const wxString& str)
{
    GdkColor colGDK;
    if ( gdk_color_parse( wxGTK_CONV_SYS( str ), &colGDK ) )
    {
        *this = wxColour(colGDK);
        return true;
    }

    return wxColourBase::FromString(str);
}
