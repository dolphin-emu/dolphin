/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/region.cpp
// Purpose:
// Author:      Robert Roebling
// Modified:    VZ at 05.10.00: use AllocExclusive(), comparison fixed
// Copyright:   (c) 1998 Robert Roebling
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

#include "wx/region.h"

#include <gdk/gdk.h>

// ----------------------------------------------------------------------------
// wxRegionRefData: private class containing the information about the region
// ----------------------------------------------------------------------------

class wxRegionRefData : public wxGDIRefData
{
public:
    wxRegionRefData()
    {
        m_region = NULL;
    }

    wxRegionRefData(const wxRegionRefData& refData)
        : wxGDIRefData()
    {
#ifdef __WXGTK3__
        m_region = cairo_region_copy(refData.m_region);
#else
        m_region = gdk_region_copy(refData.m_region);
#endif
    }

    virtual ~wxRegionRefData()
    {
        if (m_region)
        {
#ifdef __WXGTK3__
            cairo_region_destroy(m_region);
#else
            gdk_region_destroy( m_region );
#endif
        }
    }

#ifdef __WXGTK3__
    cairo_region_t* m_region;
#else
    GdkRegion  *m_region;
#endif
};

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

#define M_REGIONDATA static_cast<wxRegionRefData*>(m_refData)
#define M_REGIONDATA_OF(r) static_cast<wxRegionRefData*>(r.m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxRegion, wxGDIObject);
wxIMPLEMENT_DYNAMIC_CLASS(wxRegionIterator, wxObject);

// ----------------------------------------------------------------------------
// wxRegion construction
// ----------------------------------------------------------------------------

void wxRegion::InitRect(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
{
    GdkRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;

    m_refData = new wxRegionRefData();

#ifdef __WXGTK3__
    M_REGIONDATA->m_region = cairo_region_create_rectangle(&rect);
#else
    M_REGIONDATA->m_region = gdk_region_rectangle( &rect );
#endif
}

#ifndef __WXGTK3__
wxRegion::wxRegion(const GdkRegion* region)
{
    m_refData = new wxRegionRefData();
    M_REGIONDATA->m_region = gdk_region_copy(const_cast<GdkRegion*>(region));
}
#endif

wxRegion::wxRegion( size_t n, const wxPoint *points,
                    wxPolygonFillMode fillStyle )
{
#ifdef __WXGTK3__
    // Make a cairo path from the points, draw it onto an image surface, use
    // gdk_cairo_region_create_from_surface() to get a cairo region

    // need at least 3 points to make a useful polygon
    if (n < 3)
        return;
    // get bounding rect
    int min_x = points[0].x;
    int max_x = min_x;
    int min_y = points[0].y;
    int max_y = min_y;
    size_t i;
    for (i = 1; i < n; i++)
    {
        const int x = points[i].x;
        if (min_x > x)
            min_x = x;
        else if (max_x < x)
            max_x = x;
        const int y = points[i].y;
        if (min_y > y)
            min_y = y;
        else if (max_y < y)
            max_y = y;
    }
    const int w = max_x - min_x + 1;
    const int h = max_y - min_y + 1;
    // make surface just big enough to contain polygon (A1 is native format
    //   for gdk_cairo_region_create_from_surface)
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_A1, w, h);
    memset(cairo_image_surface_get_data(surface), 0, cairo_image_surface_get_stride(surface) * h);
    cairo_surface_mark_dirty(surface);
    cairo_surface_set_device_offset(surface, -min_x, -min_y);
    cairo_t* cr = cairo_create(surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    if (fillStyle == wxODDEVEN_RULE)
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    // make path
    cairo_move_to(cr, points[0].x, points[0].y);
    for (i = 1; i < n; i++)
        cairo_line_to(cr, points[i].x, points[i].y);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    m_refData = new wxRegionRefData;
    M_REGIONDATA->m_region = gdk_cairo_region_create_from_surface(surface);
    cairo_surface_destroy(surface);
#else
    GdkPoint *gdkpoints = new GdkPoint[n];
    for ( size_t i = 0 ; i < n ; i++ )
    {
        gdkpoints[i].x = points[i].x;
        gdkpoints[i].y = points[i].y;
    }

    m_refData = new wxRegionRefData();

    GdkRegion* reg = gdk_region_polygon
                     (
                        gdkpoints,
                        n,
                        fillStyle == wxWINDING_RULE ? GDK_WINDING_RULE
                                                    : GDK_EVEN_ODD_RULE
                     );

    M_REGIONDATA->m_region = reg;

    delete [] gdkpoints;
#endif
}

wxRegion::~wxRegion()
{
    // m_refData unrefed in ~wxObject
}

wxGDIRefData *wxRegion::CreateGDIRefData() const
{
    // should never be called
    wxFAIL;
    return NULL;
}

wxGDIRefData *wxRegion::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxRegionRefData(*static_cast<const wxRegionRefData*>(data));
}

// ----------------------------------------------------------------------------
// wxRegion comparison
// ----------------------------------------------------------------------------

bool wxRegion::DoIsEqual(const wxRegion& region) const
{
#ifdef __WXGTK3__
    return cairo_region_equal(
        M_REGIONDATA->m_region, M_REGIONDATA_OF(region)->m_region);
#else
    return gdk_region_equal(M_REGIONDATA->m_region,
                            M_REGIONDATA_OF(region)->m_region) != 0;
#endif
}

// ----------------------------------------------------------------------------
// wxRegion operations
// ----------------------------------------------------------------------------

void wxRegion::Clear()
{
    UnRef();
}

bool wxRegion::DoUnionWithRect(const wxRect& r)
{
    // workaround for a strange GTK/X11 bug: taking union with an empty
    // rectangle results in an empty region which is definitely not what we
    // want
    if ( r.IsEmpty() )
        return true;

    if ( !m_refData )
    {
        InitRect(r.x, r.y, r.width, r.height);
    }
    else
    {
        AllocExclusive();

        GdkRectangle rect;
        rect.x = r.x;
        rect.y = r.y;
        rect.width = r.width;
        rect.height = r.height;

#ifdef __WXGTK3__
        cairo_region_union_rectangle(M_REGIONDATA->m_region, &rect);
#else
        gdk_region_union_with_rect( M_REGIONDATA->m_region, &rect );
#endif
    }

    return true;
}

bool wxRegion::DoUnionWithRegion( const wxRegion& region )
{
    if (region.m_refData == NULL)
        { }
    else if (m_refData == NULL)
    {
        m_refData = new wxRegionRefData(*M_REGIONDATA_OF(region));
    }
    else
    {
        AllocExclusive();
#ifdef __WXGTK3__
        cairo_region_union(M_REGIONDATA->m_region, M_REGIONDATA_OF(region)->m_region);
#else
        gdk_region_union( M_REGIONDATA->m_region, region.GetRegion() );
#endif
    }

    return true;
}

bool wxRegion::DoIntersect( const wxRegion& region )
{
    if (region.m_refData == NULL || m_refData == NULL)
        return false;

    AllocExclusive();

#ifdef __WXGTK3__
    cairo_region_intersect(M_REGIONDATA->m_region, M_REGIONDATA_OF(region)->m_region);
#else
    gdk_region_intersect( M_REGIONDATA->m_region, region.GetRegion() );
#endif

    return true;
}

bool wxRegion::DoSubtract( const wxRegion& region )
{
    if (region.m_refData == NULL || m_refData == NULL)
        return false;

    AllocExclusive();

#ifdef __WXGTK3__
    cairo_region_subtract(M_REGIONDATA->m_region, M_REGIONDATA_OF(region)->m_region);
#else
    gdk_region_subtract( M_REGIONDATA->m_region, region.GetRegion() );
#endif

    return true;
}

bool wxRegion::DoXor( const wxRegion& region )
{
    if (region.m_refData == NULL)
        { }
    else if (m_refData == NULL)
    {
        // XOR-ing with an invalid region is the same as XOR-ing with an empty
        // one, i.e. it is simply a copy.
        m_refData = new wxRegionRefData(*M_REGIONDATA_OF(region));
    }
    else
    {
        AllocExclusive();

#ifdef __WXGTK3__
        cairo_region_xor(M_REGIONDATA->m_region, M_REGIONDATA_OF(region)->m_region);
#else
        gdk_region_xor( M_REGIONDATA->m_region, region.GetRegion() );
#endif
    }

    return true;
}

bool wxRegion::DoOffset( wxCoord x, wxCoord y )
{
    wxCHECK_MSG( m_refData, false, wxS("invalid region") );

    AllocExclusive();

#ifdef __WXGTK3__
    cairo_region_translate(M_REGIONDATA->m_region, x, y);
#else
    gdk_region_offset( M_REGIONDATA->m_region, x, y );
#endif

    return true;
}

// ----------------------------------------------------------------------------
// wxRegion tests
// ----------------------------------------------------------------------------

bool wxRegion::DoGetBox( wxCoord &x, wxCoord &y, wxCoord &w, wxCoord &h ) const
{
    if ( m_refData )
    {
        GdkRectangle rect;
#ifdef __WXGTK3__
        cairo_region_get_extents(M_REGIONDATA->m_region, &rect);
#else
        gdk_region_get_clipbox( M_REGIONDATA->m_region, &rect );
#endif
        x = rect.x;
        y = rect.y;
        w = rect.width;
        h = rect.height;

        return true;
    }
    else
    {
        x = 0;
        y = 0;
        w = -1;
        h = -1;

        return false;
    }
}

bool wxRegion::IsEmpty() const
{
#ifdef __WXGTK3__
    return m_refData == NULL || cairo_region_is_empty(M_REGIONDATA->m_region);
#else
    return m_refData == NULL || gdk_region_empty(M_REGIONDATA->m_region);
#endif
}

wxRegionContain wxRegion::DoContainsPoint( wxCoord x, wxCoord y ) const
{
#ifdef __WXGTK3__
    if (m_refData == NULL || !cairo_region_contains_point(M_REGIONDATA->m_region, x, y))
#else
    if (m_refData == NULL || !gdk_region_point_in(M_REGIONDATA->m_region, x, y))
#endif
        return wxOutRegion;

    return wxInRegion;
}

wxRegionContain wxRegion::DoContainsRect(const wxRect& r) const
{
    if (!m_refData)
        return wxOutRegion;

    GdkRectangle rect;
    rect.x = r.x;
    rect.y = r.y;
    rect.width = r.width;
    rect.height = r.height;
#ifdef __WXGTK3__
    switch (cairo_region_contains_rectangle(M_REGIONDATA->m_region, &rect))
    {
        case CAIRO_REGION_OVERLAP_IN:   return wxInRegion;
        case CAIRO_REGION_OVERLAP_PART: return wxPartRegion;
        default: break;
    }
#else
    GdkOverlapType res = gdk_region_rect_in( M_REGIONDATA->m_region, &rect );
    switch (res)
    {
        case GDK_OVERLAP_RECTANGLE_IN:   return wxInRegion;
        case GDK_OVERLAP_RECTANGLE_OUT:  return wxOutRegion;
        case GDK_OVERLAP_RECTANGLE_PART: return wxPartRegion;
    }
#endif
    return wxOutRegion;
}

#ifdef __WXGTK3__
cairo_region_t* wxRegion::GetRegion() const
#else
GdkRegion *wxRegion::GetRegion() const
#endif
{
    if (!m_refData)
        return NULL;

    return M_REGIONDATA->m_region;
}

// ----------------------------------------------------------------------------
// wxRegionIterator
// ----------------------------------------------------------------------------

wxRegionIterator::wxRegionIterator()
{
    Init();
    Reset();
}

wxRegionIterator::wxRegionIterator( const wxRegion& region )
{
    Init();
    Reset(region);
}

void wxRegionIterator::Init()
{
    m_rects = NULL;
    m_numRects = 0;
}

wxRegionIterator::~wxRegionIterator()
{
    wxDELETEA(m_rects);
}

void wxRegionIterator::CreateRects( const wxRegion& region )
{
    wxDELETEA(m_rects);
    m_numRects = 0;

#ifdef __WXGTK3__
    cairo_region_t* cairoRegion = region.GetRegion();
    if (cairoRegion == NULL)
        return;
    m_numRects = cairo_region_num_rectangles(cairoRegion);
     
    if (m_numRects)
    {
        m_rects = new wxRect[m_numRects];
        for (int i = 0; i < m_numRects; i++)
        {
            GdkRectangle gr;
            cairo_region_get_rectangle(cairoRegion, i, &gr);
            wxRect &wr = m_rects[i];
            wr.x = gr.x;
            wr.y = gr.y;
            wr.width = gr.width;
            wr.height = gr.height;
        }
    }
#else
    GdkRegion *gdkregion = region.GetRegion();
    if (!gdkregion)
        return;

    GdkRectangle* gdkrects;
    gdk_region_get_rectangles(gdkregion, &gdkrects, &m_numRects);

    if (m_numRects)
    {
        m_rects = new wxRect[m_numRects];
        for (int i = 0; i < m_numRects; ++i)
        {
            GdkRectangle &gr = gdkrects[i];
            wxRect &wr = m_rects[i];
            wr.x = gr.x;
            wr.y = gr.y;
            wr.width = gr.width;
            wr.height = gr.height;
        }
    }
    g_free( gdkrects );
#endif
}

void wxRegionIterator::Reset( const wxRegion& region )
{
    m_region = region;
    CreateRects(region);
    Reset();
}

bool wxRegionIterator::HaveRects() const
{
    return m_current < m_numRects;
}

wxRegionIterator& wxRegionIterator::operator ++ ()
{
    if (HaveRects())
        ++m_current;

    return *this;
}

wxRegionIterator wxRegionIterator::operator ++ (int)
{
    wxRegionIterator tmp = *this;

    if (HaveRects())
        ++m_current;

    return tmp;
}

wxCoord wxRegionIterator::GetX() const
{
    wxCHECK_MSG( HaveRects(), 0, wxT("invalid wxRegionIterator") );

    return m_rects[m_current].x;
}

wxCoord wxRegionIterator::GetY() const
{
    wxCHECK_MSG( HaveRects(), 0, wxT("invalid wxRegionIterator") );

    return m_rects[m_current].y;
}

wxCoord wxRegionIterator::GetW() const
{
    wxCHECK_MSG( HaveRects(), 0, wxT("invalid wxRegionIterator") );

    return m_rects[m_current].width;
}

wxCoord wxRegionIterator::GetH() const
{
    wxCHECK_MSG( HaveRects(), 0, wxT("invalid wxRegionIterator") );

    return m_rects[m_current].height;
}

wxRect wxRegionIterator::GetRect() const
{
    wxRect r;
    if( HaveRects() )
        r = m_rects[m_current];

    return r;
}

wxRegionIterator& wxRegionIterator::operator=(const wxRegionIterator& ri)
{
    if (this != &ri)
    {
        wxDELETEA(m_rects);

        m_current = ri.m_current;
        m_numRects = ri.m_numRects;
        if ( m_numRects )
        {
            m_rects = new wxRect[m_numRects];
            memcpy(m_rects, ri.m_rects, m_numRects * sizeof m_rects[0]);
        }
    }
    return *this;
}
