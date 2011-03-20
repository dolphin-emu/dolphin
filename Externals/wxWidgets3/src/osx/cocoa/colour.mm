/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/colour.mm
// Purpose:     Cocoa additions to wxColour class
// Author:      Kevin Ollivier
// Modified by:
// Created:     2009-10-31
// RCS-ID:      $Id: colour.mm 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) Kevin Ollivier
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/colour.h"

#ifndef WX_PRECOMP
    #include "wx/gdicmn.h"
#endif

#include "wx/osx/private.h"

wxColour::wxColour(WX_NSColor col)
{
    size_t noComp = [col numberOfComponents];

    CGFloat components[4];
    CGFloat *p;
    if ( noComp < 1 || noComp > WXSIZEOF(components) )
    {
        // TODO verify whether we really are on a RGB color space
        m_alpha = wxALPHA_OPAQUE;
        [col getComponents: components];
        p = components;
    }
    else // Unsupported colour format.
    {
        p = NULL;
    }

    InitFromComponents(components, noComp);
}

WX_NSColor wxColour::OSXGetNSColor() const
{
    return [NSColor colorWithDeviceRed:m_red / 255.0 green:m_green / 255.0 blue:m_blue / 255.0 alpha:m_alpha / 255.0];
}
