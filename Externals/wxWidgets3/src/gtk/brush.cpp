/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/brush.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: brush.cpp 61508 2009-07-23 20:30:22Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/brush.h"

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/colour.h"
#endif

//-----------------------------------------------------------------------------
// wxBrush
//-----------------------------------------------------------------------------

class wxBrushRefData: public wxGDIRefData
{
public:
    wxBrushRefData(const wxColour& colour = wxNullColour, wxBrushStyle style = wxBRUSHSTYLE_SOLID)
    {
        m_style = style;
        m_colour = colour;
    }

    wxBrushRefData( const wxBrushRefData& data )
        : wxGDIRefData()
    {
        m_style = data.m_style;
        m_stipple = data.m_stipple;
        m_colour = data.m_colour;
    }

    bool operator == (const wxBrushRefData& data) const
    {
        return (m_style == data.m_style &&
                m_stipple.IsSameAs(data.m_stipple) &&
                m_colour == data.m_colour);
    }

    wxBrushStyle m_style;
    wxColour     m_colour;
    wxBitmap     m_stipple;
};

//-----------------------------------------------------------------------------

#define M_BRUSHDATA ((wxBrushRefData *)m_refData)

IMPLEMENT_DYNAMIC_CLASS(wxBrush,wxGDIObject)

wxBrush::wxBrush( const wxColour &colour, wxBrushStyle style )
{
    m_refData = new wxBrushRefData(colour, style);
}

#if FUTURE_WXWIN_COMPATIBILITY_3_0
wxBrush::wxBrush(const wxColour& col, int style)
{
    m_refData = new wxBrushRefData(col, (wxBrushStyle)style);
}
#endif

wxBrush::wxBrush( const wxBitmap &stippleBitmap )
{
    wxBrushStyle style = wxBRUSHSTYLE_STIPPLE;
    if (stippleBitmap.GetMask())
        style = wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE;

    m_refData = new wxBrushRefData(*wxBLACK, style);
    M_BRUSHDATA->m_stipple = stippleBitmap;
}

wxBrush::~wxBrush()
{
    // m_refData unrefed in ~wxObject
}

wxGDIRefData *wxBrush::CreateGDIRefData() const
{
    return new wxBrushRefData;
}

wxGDIRefData *wxBrush::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxBrushRefData(*(wxBrushRefData *)data);
}

bool wxBrush::operator==(const wxBrush& brush) const
{
    if (m_refData == brush.m_refData) return true;

    if (!m_refData || !brush.m_refData) return false;

    return ( *(wxBrushRefData*)m_refData == *(wxBrushRefData*)brush.m_refData );
}

wxBrushStyle wxBrush::GetStyle() const
{
    wxCHECK_MSG( Ok(), wxBRUSHSTYLE_INVALID, wxT("invalid brush") );

    return M_BRUSHDATA->m_style;
}

wxColour wxBrush::GetColour() const
{
    wxCHECK_MSG( Ok(), wxNullColour, wxT("invalid brush") );

    return M_BRUSHDATA->m_colour;
}

wxBitmap *wxBrush::GetStipple() const
{
    wxCHECK_MSG( Ok(), NULL, wxT("invalid brush") );

    return &M_BRUSHDATA->m_stipple;
}

void wxBrush::SetColour( const wxColour& col )
{
    AllocExclusive();

    M_BRUSHDATA->m_colour = col;
}

void wxBrush::SetColour( unsigned char r, unsigned char g, unsigned char b )
{
    AllocExclusive();

    M_BRUSHDATA->m_colour.Set( r, g, b );
}

void wxBrush::SetStyle( wxBrushStyle style )
{
    AllocExclusive();

    M_BRUSHDATA->m_style = style;
}

void wxBrush::SetStipple( const wxBitmap& stipple )
{
    AllocExclusive();

    M_BRUSHDATA->m_stipple = stipple;
    if (M_BRUSHDATA->m_stipple.GetMask())
    {
        M_BRUSHDATA->m_style = wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE;
    }
    else
    {
        M_BRUSHDATA->m_style = wxBRUSHSTYLE_STIPPLE;
    }
}
