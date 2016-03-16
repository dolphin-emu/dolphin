/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/colour.cpp
// Purpose:     wxColour class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/colour.h"

#ifndef WX_PRECOMP
    #include "wx/gdicmn.h"
#endif

#include "wx/osx/private.h"

#if wxOSX_USE_COCOA_OR_CARBON
wxColour::wxColour(const RGBColor& col)
{
    InitRGBColor(col);
}
#endif

wxColour::wxColour(CGColorRef col)
{
    InitCGColorRef(col);
}

#if wxOSX_USE_COCOA_OR_CARBON
void wxColour::GetRGBColor( RGBColor *col ) const
{
    col->red = (m_red << 8) + m_red;
    col->blue = (m_blue << 8) + m_blue;
    col->green = (m_green << 8) + m_green;
}

wxColour& wxColour::operator=(const RGBColor& col)
{
    InitRGBColor(col);
    return *this;
}
#endif

wxColour& wxColour::operator=(CGColorRef col)
{
    InitCGColorRef(col);
    return *this;
}

wxColour& wxColour::operator=(const wxColour& col)
{
    m_red = col.m_red;
    m_green = col.m_green;
    m_blue = col.m_blue;
    m_alpha = col.m_alpha;
    m_cgColour = col.m_cgColour;
    return *this;
}

void wxColour::InitRGBA (ChannelType r, ChannelType g, ChannelType b, ChannelType a)
{
    m_red = r;
    m_green = g;
    m_blue = b;
    m_alpha = a ;

    CGColorRef col = 0 ;
#if wxOSX_USE_COCOA_OR_CARBON 
    col = CGColorCreateGenericRGB( (CGFloat)(r / 255.0), (CGFloat) (g / 255.0), (CGFloat) (b / 255.0), (CGFloat) (a / 255.0) );
#else
    CGFloat components[4] = { (CGFloat)(r / 255.0), (CGFloat) (g / 255.0), (CGFloat) (b / 255.0), (CGFloat) (a / 255.0) } ;
    col = CGColorCreate( wxMacGetGenericRGBColorSpace() , components ) ;
#endif
    wxASSERT_MSG(col != NULL, "Invalid CoreGraphics Color");
    m_cgColour.reset( col );
}

#if wxOSX_USE_COCOA_OR_CARBON
void wxColour::InitRGBColor( const RGBColor& col )
{
    m_red = col.red >> 8;
    m_blue = col.blue >> 8;
    m_green = col.green >> 8;
    m_alpha = wxALPHA_OPAQUE;
    CGColorRef cfcol;
    cfcol = CGColorCreateGenericRGB((CGFloat)(col.red / 65535.0), (CGFloat)(col.green / 65535.0),
                                        (CGFloat)(col.blue / 65535.0), (CGFloat) 1.0 );
    wxASSERT_MSG(cfcol != NULL, "Invalid CoreGraphics Color");
    m_cgColour.reset( cfcol );
}
#endif

void wxColour::InitCGColorRef( CGColorRef col )
{
    wxASSERT_MSG(col != NULL, "Invalid CoreGraphics Color");
    m_cgColour.reset( col );
    size_t noComp = CGColorGetNumberOfComponents( col );

    const CGFloat *components = NULL;
    if ( noComp >= 1 && noComp <= 4 )
    {
        // TODO verify whether we really are on a RGB color space
        m_alpha = wxALPHA_OPAQUE;
        components = CGColorGetComponents( col );
    }

    if ( noComp < 1 || !components )
    {
        m_alpha = wxALPHA_OPAQUE;
        m_red = m_green = m_blue = 0;
        return;
    }

    if ( noComp >= 3 )
    {
        m_red = (int)(components[0]*255+0.5);
        m_green = (int)(components[1]*255+0.5);
        m_blue = (int)(components[2]*255+0.5);
        if ( noComp == 4 )
            m_alpha =  (int)(components[3]*255+0.5);
    }
    else
    {
        m_red = (int)(components[0]*255+0.5);
        m_green = (int)(components[0]*255+0.5);
        m_blue = (int)(components[0]*255+0.5);
    }
}

bool wxColour::operator == (const wxColour& colour) const
{
    return ( (IsOk() == colour.IsOk()) && (!IsOk() ||
                                           CGColorEqualToColor( m_cgColour, colour.m_cgColour ) ) );
}


