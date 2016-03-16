/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/pen.cpp
// Purpose:     wxPen
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/pen.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxPen, wxGDIObject);

class WXDLLEXPORT wxPenRefData : public wxGDIRefData
{
public:
    wxPenRefData();
    wxPenRefData(const wxPenRefData& data);
    virtual ~wxPenRefData();

    wxPenRefData& operator=(const wxPenRefData& data);

    bool operator==(const wxPenRefData& data) const
    {
        // we intentionally don't compare m_hPen fields here
        return m_style == data.m_style &&
            m_width == data.m_width &&
            m_join == data.m_join &&
            m_cap == data.m_cap &&
            m_colour == data.m_colour &&
            (m_style != wxPENSTYLE_STIPPLE || m_stipple.IsSameAs(data.m_stipple)) &&
            (m_style != wxPENSTYLE_USER_DASH ||
             (m_nbDash == data.m_nbDash &&
              memcmp(m_dash, data.m_dash, m_nbDash*sizeof(wxDash)) == 0));
    }

protected:
    int           m_width;
    wxPenStyle    m_style;
    wxPenJoin     m_join ;
    wxPenCap      m_cap ;
    wxBitmap      m_stipple ;
    int           m_nbDash ;
    wxDash *      m_dash ;
    wxColour      m_colour;
    /* TODO: implementation
       WXHPEN        m_hPen;
       */

    friend class WXDLLIMPEXP_FWD_CORE wxPen;
};

wxPenRefData::wxPenRefData()
{
    m_style = wxPENSTYLE_SOLID;
    m_width = 1;
    m_join = wxJOIN_ROUND ;
    m_cap = wxCAP_ROUND ;
    m_nbDash = 0 ;
    m_dash = 0 ;
}

wxPenRefData::wxPenRefData(const wxPenRefData& data)
: wxGDIRefData()
{
    m_style = data.m_style;
    m_width = data.m_width;
    m_join = data.m_join;
    m_cap = data.m_cap;
    m_nbDash = data.m_nbDash;
    m_dash = data.m_dash;
    m_colour = data.m_colour;
}

wxPenRefData::~wxPenRefData()
{
}

// Pens

#define M_PENDATA ((wxPenRefData *)m_refData)

wxPen::wxPen()
{
}

wxPen::~wxPen()
{
}

// Should implement Create
wxPen::wxPen(const wxColour& col, int Width, wxPenStyle Style)
{
    m_refData = new wxPenRefData;

    M_PENDATA->m_colour = col;
    M_PENDATA->m_width = Width;
    M_PENDATA->m_style = Style;
    M_PENDATA->m_join = wxJOIN_ROUND ;
    M_PENDATA->m_cap = wxCAP_ROUND ;
    M_PENDATA->m_nbDash = 0 ;
    M_PENDATA->m_dash = 0 ;

    RealizeResource();
}

wxPen::wxPen(const wxColour& col, int Width, int Style)
{
    m_refData = new wxPenRefData;

    M_PENDATA->m_colour = col;
    M_PENDATA->m_width = Width;
    M_PENDATA->m_style = (wxPenStyle)Style;
    M_PENDATA->m_join = wxJOIN_ROUND ;
    M_PENDATA->m_cap = wxCAP_ROUND ;
    M_PENDATA->m_nbDash = 0 ;
    M_PENDATA->m_dash = 0 ;

    RealizeResource();
}

wxPen::wxPen(const wxBitmap& stipple, int Width)
{
    m_refData = new wxPenRefData;

    M_PENDATA->m_stipple = stipple;
    M_PENDATA->m_width = Width;
    M_PENDATA->m_style = wxPENSTYLE_STIPPLE;
    M_PENDATA->m_join = wxJOIN_ROUND ;
    M_PENDATA->m_cap = wxCAP_ROUND ;
    M_PENDATA->m_nbDash = 0 ;
    M_PENDATA->m_dash = 0 ;

    RealizeResource();
}

wxGDIRefData *wxPen::CreateGDIRefData() const
{
    return new wxPenRefData;
}

wxGDIRefData *wxPen::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxPenRefData(*static_cast<const wxPenRefData *>(data));
}

bool wxPen::operator==(const wxPen& pen) const
{
    const wxPenRefData *penData = (wxPenRefData *)pen.m_refData;

    // an invalid pen is only equal to another invalid pen
    return m_refData ? penData && *M_PENDATA == *penData : !penData;
}

wxColour wxPen::GetColour() const
{
    wxCHECK_MSG( IsOk(), wxNullColour, wxT("invalid pen") );

    return M_PENDATA->m_colour;
}

int wxPen::GetWidth() const
{
    wxCHECK_MSG( IsOk(), -1, wxT("invalid pen") );

    return M_PENDATA->m_width;
}

wxPenStyle wxPen::GetStyle() const
{
    wxCHECK_MSG( IsOk(), wxPENSTYLE_INVALID, wxT("invalid pen") );

    return M_PENDATA->m_style;
}

wxPenJoin wxPen::GetJoin() const
{
    wxCHECK_MSG( IsOk(), wxJOIN_INVALID, wxT("invalid pen") );

    return M_PENDATA->m_join;
}

wxPenCap wxPen::GetCap() const
{
    wxCHECK_MSG( IsOk(), wxCAP_INVALID, wxT("invalid pen") );

    return M_PENDATA->m_cap;
}

int wxPen::GetDashes(wxDash **ptr) const
{
    wxCHECK_MSG( IsOk(), -1, wxT("invalid pen") );

    *ptr = M_PENDATA->m_dash;
    return M_PENDATA->m_nbDash;
}

int wxPen::GetDashCount() const
{
    return M_PENDATA->m_nbDash;
}

wxBitmap *wxPen::GetStipple() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid pen") );

    return &M_PENDATA->m_stipple;
}

void wxPen::Unshare()
{
    // Don't change shared data
    if (!m_refData)
    {
        m_refData = new wxPenRefData();
    }
    else
    {
        wxPenRefData* ref = new wxPenRefData(*(wxPenRefData*)m_refData);
        UnRef();
        m_refData = ref;
    }
}

void wxPen::SetColour(const wxColour& col)
{
    Unshare();

    M_PENDATA->m_colour = col;

    RealizeResource();
}

void wxPen::SetColour(unsigned char r, unsigned char g, unsigned char b)
{
    Unshare();

    M_PENDATA->m_colour.Set(r, g, b);

    RealizeResource();
}

void wxPen::SetWidth(int Width)
{
    Unshare();

    M_PENDATA->m_width = Width;

    RealizeResource();
}

void wxPen::SetStyle(wxPenStyle Style)
{
    Unshare();

    M_PENDATA->m_style = Style;

    RealizeResource();
}

void wxPen::SetStipple(const wxBitmap& Stipple)
{
    Unshare();

    M_PENDATA->m_stipple = Stipple;
    M_PENDATA->m_style = wxPENSTYLE_STIPPLE;

    RealizeResource();
}

void wxPen::SetDashes(int nb_dashes, const wxDash *Dash)
{
    Unshare();

    M_PENDATA->m_nbDash = nb_dashes;
    M_PENDATA->m_dash = (wxDash *)Dash;

    RealizeResource();
}

void wxPen::SetJoin(wxPenJoin Join)
{
    Unshare();

    M_PENDATA->m_join = Join;

    RealizeResource();
}

void wxPen::SetCap(wxPenCap Cap)
{
    Unshare();

    M_PENDATA->m_cap = Cap;

    RealizeResource();
}

bool wxPen::RealizeResource()
{
    // nothing to do here for mac
    return true;
}
