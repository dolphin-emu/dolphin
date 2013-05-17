/////////////////////////////////////////////////////////////////////////////
// File:      src/osx/carbon/region.cpp
// Purpose:   Region class
// Author:    Stefan Csomor
// Created:   Fri Oct 24 10:46:34 MET 1997
// RCS-ID:    $Id: region.cpp 70537 2012-02-08 00:20:38Z RD $
// Copyright: (c) 1997 Stefan Csomor
// Licence:   wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxOSX_USE_COCOA_OR_CARBON

#include "wx/region.h"

#ifndef WX_PRECOMP
    #include "wx/gdicmn.h"
    #include "wx/dcmemory.h"
#endif

#include "wx/osx/private.h"

IMPLEMENT_DYNAMIC_CLASS(wxRegion, wxGDIObject)
IMPLEMENT_DYNAMIC_CLASS(wxRegionIterator, wxObject)

//-----------------------------------------------------------------------------
// wxRegionRefData implementation
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxRegionRefData : public wxGDIRefData
{
public:
    wxRegionRefData()
    {
        m_macRgn.reset( HIShapeCreateMutable() );
    }

    wxRegionRefData(wxCFRef<HIShapeRef> &region)
    {
        m_macRgn.reset( HIShapeCreateMutableCopy(region) );
    }

    wxRegionRefData(long x, long y, long w, long h)
    {
        CGRect r = CGRectMake(x,y,w,h);
        wxCFRef<HIShapeRef> rect(HIShapeCreateWithRect(&r));
        m_macRgn.reset( HIShapeCreateMutableCopy(rect) );
    }

    wxRegionRefData(const wxRegionRefData& data)
        : wxGDIRefData()
    {
        m_macRgn.reset( HIShapeCreateMutableCopy(data.m_macRgn) );
    }

    virtual ~wxRegionRefData()
    {
    }

    wxCFRef<HIMutableShapeRef> m_macRgn;
};

#define M_REGION (((wxRegionRefData*)m_refData)->m_macRgn)
#define OTHER_M_REGION(a) (((wxRegionRefData*)(a.m_refData))->m_macRgn)

//-----------------------------------------------------------------------------
// wxRegion
//-----------------------------------------------------------------------------

wxRegion::wxRegion(WXHRGN hRegion )
{
    wxCFRef< HIShapeRef > shape( (HIShapeRef) hRegion );
    m_refData = new wxRegionRefData(shape);
}

wxRegion::wxRegion(long x, long y, long w, long h)
{
    m_refData = new wxRegionRefData(x , y , w , h );
}

wxRegion::wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight)
{
    m_refData = new wxRegionRefData(topLeft.x , topLeft.y ,
                                    bottomRight.x - topLeft.x,
                                    bottomRight.y - topLeft.y);
}

wxRegion::wxRegion(const wxRect& rect)
{
    m_refData = new wxRegionRefData(rect.x , rect.y , rect.width , rect.height);
}

wxRegion::wxRegion(size_t n, const wxPoint *points, wxPolygonFillMode fillStyle)
{
    // Set the region to a polygon shape generically using a bitmap with the 
    // polygon drawn on it. 
 
    m_refData = new wxRegionRefData(); 
     
    wxCoord mx = 0; 
    wxCoord my = 0; 
    wxPoint p; 
    size_t idx;     
     
    // Find the max size needed to draw the polygon 
    for (idx=0; idx<n; idx++) 
    { 
        wxPoint pt = points[idx]; 
        if (pt.x > mx) 
            mx = pt.x; 
        if (pt.y > my) 
            my = pt.y; 
    } 
 
    // Make the bitmap 
    wxBitmap bmp(mx, my); 
    wxMemoryDC dc(bmp); 
    dc.SetBackground(*wxBLACK_BRUSH); 
    dc.Clear(); 
    dc.SetPen(*wxWHITE_PEN); 
    dc.SetBrush(*wxWHITE_BRUSH); 
    dc.DrawPolygon(n, (wxPoint*)points, 0, 0, fillStyle); 
    dc.SelectObject(wxNullBitmap); 
    bmp.SetMask(new wxMask(bmp, *wxBLACK)); 
 
    // Use it to set this region 
    Union(bmp); 
}

wxRegion::~wxRegion()
{
    // m_refData unrefed in ~wxObject
}

wxGDIRefData *wxRegion::CreateGDIRefData() const
{
    return new wxRegionRefData;
}

wxGDIRefData *wxRegion::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxRegionRefData(*static_cast<const wxRegionRefData *>(data));
}

//-----------------------------------------------------------------------------
//# Modify region
//-----------------------------------------------------------------------------

//! Clear current region
void wxRegion::Clear()
{
    UnRef();
}

// Move the region
bool wxRegion::DoOffset(wxCoord x, wxCoord y)
{
    wxCHECK_MSG( m_refData, false, wxT("invalid wxRegion") );

    if ( !x && !y )
        // nothing to do
        return true;

    AllocExclusive();

    verify_noerr( HIShapeOffset( M_REGION , x , y ) ) ;

    return true ;
}


//! Union /e region with this.
bool wxRegion::DoCombine(const wxRegion& region, wxRegionOp op)
{
    wxCHECK_MSG( region.IsOk(), false, wxT("invalid wxRegion") );

    // Handle the special case of not initialized (e.g. default constructed)
    // region as we can't use HIShape functions if we don't have any shape.
    if ( !m_refData )
    {
        switch ( op )
        {
            case wxRGN_COPY:
            case wxRGN_OR:
            case wxRGN_XOR:
                // These operations make sense with a null region.
                *this = region;
                return true;

            case wxRGN_AND:
            case wxRGN_DIFF:
                // Those ones don't really make sense so just leave this region
                // empty/invalid.
                return false;
        }

        wxFAIL_MSG( wxT("Unknown region operation") );
        return false;
    }

    AllocExclusive();

    switch (op)
    {
        case wxRGN_AND:
            verify_noerr( HIShapeIntersect( M_REGION , OTHER_M_REGION(region) , M_REGION ) );
            break ;

        case wxRGN_OR:
            verify_noerr( HIShapeUnion( M_REGION , OTHER_M_REGION(region) , M_REGION ) );
            break ;

        case wxRGN_XOR:
            {
                // XOR is defined as the difference between union and intersection
                wxCFRef< HIShapeRef > unionshape( HIShapeCreateUnion( M_REGION , OTHER_M_REGION(region) ) );
                wxCFRef< HIShapeRef > intersectionshape( HIShapeCreateIntersection( M_REGION , OTHER_M_REGION(region) ) );
                verify_noerr( HIShapeDifference( unionshape, intersectionshape, M_REGION ) );
            }
            break ;

        case wxRGN_DIFF:
            verify_noerr( HIShapeDifference( M_REGION , OTHER_M_REGION(region) , M_REGION ) ) ;
            break ;

        case wxRGN_COPY:
        default:
            M_REGION.reset( HIShapeCreateMutableCopy( OTHER_M_REGION(region) ) );
            break ;
    }

    return true;
}

//-----------------------------------------------------------------------------
//# Information on region
//-----------------------------------------------------------------------------

bool wxRegion::DoIsEqual(const wxRegion& region) const
{
    // There doesn't seem to be any native function for checking the equality
    // of HIShapes so we compute their differences to determine if they are
    // equal.
    wxRegion r(*this);
    r.Subtract(region);

    if ( !r.IsEmpty() )
        return false;

    wxRegion r2(region);
    r2.Subtract(*this);

    return r2.IsEmpty();
}

// Outer bounds of region
bool wxRegion::DoGetBox(wxCoord& x, wxCoord& y, wxCoord& w, wxCoord& h) const
{
    if (m_refData)
    {
        CGRect box ;
        HIShapeGetBounds( M_REGION , &box ) ;
        x = static_cast<int>(box.origin.x);
        y = static_cast<int>(box.origin.y);
        w = static_cast<int>(box.size.width);
        h = static_cast<int>(box.size.height);

        return true;
    }
    else
    {
        x = y = w = h = 0;

        return false;
    }
}

// Is region empty?
bool wxRegion::IsEmpty() const
{
    if ( m_refData )
        return HIShapeIsEmpty( M_REGION ) ;
    else
        return true ;
}

WXHRGN wxRegion::GetWXHRGN() const
{
    if ( !m_refData )
        return NULL;

    return M_REGION ;
}

//-----------------------------------------------------------------------------
//# Tests
//-----------------------------------------------------------------------------

// Does the region contain the point?
wxRegionContain wxRegion::DoContainsPoint(wxCoord x, wxCoord y) const
{
    if (!m_refData)
        return wxOutRegion;

    CGPoint p = { x, y } ;
    if (HIShapeContainsPoint( M_REGION , &p ) )
        return wxInRegion;

    return wxOutRegion;
}

// Does the region contain the rectangle (x, y, w, h)?
wxRegionContain wxRegion::DoContainsRect(const wxRect& r) const
{
    if (!m_refData)
        return wxOutRegion;

    CGRect rect = CGRectMake(r.x,r.y,r.width,r.height);
    wxCFRef<HIShapeRef> rectshape(HIShapeCreateWithRect(&rect));
    wxCFRef<HIShapeRef> intersect(HIShapeCreateIntersection(rectshape,M_REGION));
    CGRect bounds;
    HIShapeGetBounds(intersect, &bounds);

    if ( HIShapeIsRectangular(intersect) && CGRectEqualToRect(rect,bounds) )
        return wxInRegion;
    else if ( HIShapeIsEmpty( intersect ) )
        return wxOutRegion;
    else
        return wxPartRegion;
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                               wxRegionIterator                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

/*!
 * Initialize empty iterator
 */
wxRegionIterator::wxRegionIterator()
    : m_current(0), m_numRects(0), m_rects(NULL)
{
}

wxRegionIterator::~wxRegionIterator()
{
    wxDELETEA(m_rects);
}

wxRegionIterator::wxRegionIterator(const wxRegionIterator& iterator)
    : wxObject()
    , m_current(iterator.m_current)
    , m_numRects(0)
    , m_rects(NULL)
{
    SetRects(iterator.m_numRects, iterator.m_rects);
}

wxRegionIterator& wxRegionIterator::operator=(const wxRegionIterator& iterator)
{
    m_current  = iterator.m_current;
    SetRects(iterator.m_numRects, iterator.m_rects);

    return *this;
}

/*!
 * Set iterator rects for region
 */
void wxRegionIterator::SetRects(long numRects, wxRect *rects)
{
    wxDELETEA(m_rects);

    if (rects && (numRects > 0))
    {
        int i;

        m_rects = new wxRect[numRects];
        for (i = 0; i < numRects; i++)
            m_rects[i] = rects[i];
    }

    m_numRects = numRects;
}

/*!
 * Initialize iterator for region
 */
wxRegionIterator::wxRegionIterator(const wxRegion& region)
{
    m_rects = NULL;

    Reset(region);
}

/*!
 * Reset iterator for a new /e region.
 */

class RegionToRectsCallbackData
{
public :
    wxRect* m_rects ;
    long m_current ;
};

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5

OSStatus wxMacRegionToRectsCounterCallback(
    UInt16 message, RgnHandle WXUNUSED(region), const Rect *WXUNUSED(rect), void *data )
{
    long *m_numRects = (long*) data ;
    if ( message == kQDRegionToRectsMsgInit )
    {
        (*m_numRects) = 0 ;
    }
    else if (message == kQDRegionToRectsMsgParse)
    {
        (*m_numRects) += 1 ;
    }

    return noErr;
}

OSStatus wxMacRegionToRectsSetterCallback(
    UInt16 message, RgnHandle WXUNUSED(region), const Rect *rect, void *data )
{
    if (message == kQDRegionToRectsMsgParse)
    {
        RegionToRectsCallbackData *cb = (RegionToRectsCallbackData*) data ;
        cb->m_rects[cb->m_current++] = wxRect( rect->left , rect->top , rect->right - rect->left , rect->bottom - rect->top ) ;
    }

    return noErr;
}

#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5

OSStatus wxOSXRegionToRectsCounterCallback(
    int message, HIShapeRef WXUNUSED(region), const CGRect *WXUNUSED(rect), void *data )
{
    long *m_numRects = (long*) data ;
    if ( message == kHIShapeEnumerateInit )
    {
        (*m_numRects) = 0 ;
    }
    else if (message == kHIShapeEnumerateRect)
    {
        (*m_numRects) += 1 ;
    }

    return noErr;
}

OSStatus wxOSXRegionToRectsSetterCallback(
    int message, HIShapeRef WXUNUSED(region), const CGRect *rect, void *data )
{
    if (message == kHIShapeEnumerateRect)
    {
        RegionToRectsCallbackData *cb = (RegionToRectsCallbackData*) data ;
        cb->m_rects[cb->m_current++] = wxRect( rect->origin.x , rect->origin.y , rect->size.width , rect->size.height ) ;
    }

    return noErr;
}

#endif

void wxRegionIterator::Reset(const wxRegion& region)
{
    m_current = 0;
    m_region = region;

    wxDELETEA(m_rects);

    if (m_region.IsEmpty())
    {
        m_numRects = 0;
    }
    else
    {
#if 0
        // fallback code in case we ever need it again
        // copying this to a path and dissecting the path would be an option
        m_numRects = 1;
        m_rects = new wxRect[m_numRects];
        m_rects[0] = m_region.GetBox();
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
        if ( HIShapeEnumerate != NULL )
        {
            OSStatus err = HIShapeEnumerate (OTHER_M_REGION(region), kHIShapeParseFromTopLeft, wxOSXRegionToRectsCounterCallback,
                (void*)&m_numRects);
            if (err == noErr)
            {
                m_rects = new wxRect[m_numRects];
                RegionToRectsCallbackData data ;
                data.m_rects = m_rects ;
                data.m_current = 0 ;
                HIShapeEnumerate( OTHER_M_REGION(region), kHIShapeParseFromTopLeft, wxOSXRegionToRectsSetterCallback,
                    (void*)&data );
            }
            else
            {
                m_numRects = 0;
            }
        }
        else
#endif
        {
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
            OSStatus err = noErr;
            RgnHandle rgn = NewRgn();
            HIShapeGetAsQDRgn(OTHER_M_REGION(region), rgn);

            err = QDRegionToRects (rgn, kQDParseRegionFromTopLeft, wxMacRegionToRectsCounterCallback
                , (void*)&m_numRects);
            if (err == noErr)
            {
                m_rects = new wxRect[m_numRects];
                RegionToRectsCallbackData data ;
                data.m_rects = m_rects ;
                data.m_current = 0 ;
                QDRegionToRects( rgn , kQDParseRegionFromTopLeft, wxMacRegionToRectsSetterCallback,
                    (void*)&data );
            }
            else
            {
                m_numRects = 0;
            }
            DisposeRgn( rgn );
#endif
        }
    }
}

/*!
 * Increment iterator. The rectangle returned is the one after the
 * incrementation.
 */
wxRegionIterator& wxRegionIterator::operator ++ ()
{
    if (m_current < m_numRects)
        ++m_current;

    return *this;
}

/*!
 * Increment iterator. The rectangle returned is the one before the
 * incrementation.
 */
wxRegionIterator wxRegionIterator::operator ++ (int)
{
    wxRegionIterator previous(*this);

    if (m_current < m_numRects)
        ++m_current;

    return previous;
}

long wxRegionIterator::GetX() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].x;

    return 0;
}

long wxRegionIterator::GetY() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].y;

    return 0;
}

long wxRegionIterator::GetW() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].width ;

    return 0;
}

long wxRegionIterator::GetH() const
{
    if (m_current < m_numRects)
        return m_rects[m_current].height;

    return 0;
}

#endif
