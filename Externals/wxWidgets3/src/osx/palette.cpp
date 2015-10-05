/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/palette.cpp
// Purpose:     wxPalette
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_PALETTE

#include "wx/palette.h"
#include "wx/colour.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxPalette, wxGDIObject);

// ============================================================================
// wxPaletteRefData
// ============================================================================

class WXDLLEXPORT wxPaletteRefData: public wxGDIRefData
{
public:
    wxPaletteRefData();
    wxPaletteRefData(const wxPaletteRefData& data);
    virtual ~wxPaletteRefData();

    virtual bool IsOk() const wxOVERRIDE { return m_count > 0; }

protected:
    wxColour* m_palette;
    wxInt32   m_count;

    friend class WXDLLIMPEXP_FWD_CORE wxPalette;

    wxDECLARE_NO_ASSIGN_CLASS(wxPaletteRefData);
};

wxPaletteRefData::wxPaletteRefData()
{
    m_palette = NULL;
    m_count = 0;
}

wxPaletteRefData::wxPaletteRefData(const wxPaletteRefData& data) : wxGDIRefData()
{
    m_count = data.m_count;
    m_palette = new wxColour[m_count];
    for ( wxInt32 i = 0; i < m_count; i++ )
        m_palette[i] = data.m_palette[i];
}

wxPaletteRefData::~wxPaletteRefData()
{
    delete[] m_palette;
}

// ============================================================================
// wxPalette
// ============================================================================

wxPalette::wxPalette()
{
}

wxPalette::wxPalette(int n, const unsigned char *red, const unsigned char *green, const unsigned char *blue)
{
    Create(n, red, green, blue);
}

wxPalette::~wxPalette()
{
}

bool wxPalette::Create(int n, const unsigned char *red, const unsigned char *green, const unsigned char *blue)
{
    UnRef();

    m_refData = new wxPaletteRefData;

    M_PALETTEDATA->m_count = n ;
    M_PALETTEDATA->m_palette = new wxColour[n] ;

    for ( int i = 0 ; i < n ; ++i)
    {
        M_PALETTEDATA->m_palette[i].Set( red[i] , green[i] , blue[i] ) ;
    }

    return false;
}

int wxPalette::GetPixel(unsigned char red, unsigned char green, unsigned char blue) const
{
    if ( !m_refData )
        return wxNOT_FOUND;

    long bestdiff = 3 * 256 ;
    long bestpos = 0 ;
    long currentdiff ;

    for ( int i = 0  ; i < M_PALETTEDATA->m_count ; ++i )
    {
        const wxColour& col = M_PALETTEDATA->m_palette[i] ;
        currentdiff = abs ( col.Red() - red ) + abs( col.Green() - green ) + abs ( col.Blue() - blue )  ;
        if ( currentdiff < bestdiff )
        {
            bestdiff = currentdiff ;
            bestpos = i ;
            if ( bestdiff == 0 )
                break ;
        }
    }

    return bestpos;
}

bool wxPalette::GetRGB(int index, unsigned char *red, unsigned char *green, unsigned char *blue) const
{
    if ( !m_refData )
        return false;

    if (index < 0 || index >= M_PALETTEDATA->m_count)
        return false;

    const wxColour& col = M_PALETTEDATA->m_palette[index] ;
    *red = col.Red() ;
    *green = col.Green() ;
    *blue = col.Blue() ;

    return true;
}

int wxPalette::GetColoursCount() const
{
    if (m_refData)
        return M_PALETTEDATA->m_count;

    return 0;
}

wxGDIRefData *wxPalette::CreateGDIRefData() const
{
    return new wxPaletteRefData;
}

wxGDIRefData *wxPalette::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxPaletteRefData(*static_cast<const wxPaletteRefData *>(data));
}

#endif // wxUSE_PALETTE
