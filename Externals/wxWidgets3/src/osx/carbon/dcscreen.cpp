/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dcscreen.cpp
// Purpose:     wxScreenDC class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/dcscreen.h"
#include "wx/osx/dcscreen.h"

#include "wx/osx/private.h"
#include "wx/graphics.h"
#if wxOSX_USE_COCOA_OR_CARBON
#include "wx/osx/private/glgrab.h"
#endif

IMPLEMENT_ABSTRACT_CLASS(wxScreenDCImpl, wxWindowDCImpl)

// TODO : for the Screenshot use case, which doesn't work in Quartz
// we should do a GetAsBitmap using something like
// http://www.cocoabuilder.com/archive/message/cocoa/2005/8/13/144256

// Create a DC representing the whole screen
wxScreenDCImpl::wxScreenDCImpl( wxDC *owner ) :
   wxWindowDCImpl( owner )
{
#if wxOSX_USE_COCOA_OR_CARBON
    CGRect cgbounds ;
    cgbounds = CGDisplayBounds(CGMainDisplayID());
    m_width = (wxCoord)cgbounds.size.width;
    m_height = (wxCoord)cgbounds.size.height;
#else
    wxDisplaySize( &m_width, &m_height );
#endif
#if wxOSX_USE_COCOA_OR_IPHONE
    SetGraphicsContext( wxGraphicsContext::Create() );
#else
    Rect bounds;
    bounds.top = (short)cgbounds.origin.y;
    bounds.left = (short)cgbounds.origin.x;
    bounds.bottom = bounds.top + (short)cgbounds.size.height;
    bounds.right = bounds.left  + (short)cgbounds.size.width;
    WindowAttributes overlayAttributes  = kWindowIgnoreClicksAttribute;
    CreateNewWindow( kOverlayWindowClass, overlayAttributes, &bounds, (WindowRef*) &m_overlayWindow );
    ShowWindow((WindowRef)m_overlayWindow);
    SetGraphicsContext( wxGraphicsContext::CreateFromNativeWindow( m_overlayWindow ) );
#endif
    m_ok = true ;
}

wxScreenDCImpl::~wxScreenDCImpl()
{
    wxDELETE(m_graphicContext);
#if wxOSX_USE_COCOA_OR_IPHONE
#else
    DisposeWindow((WindowRef) m_overlayWindow );
#endif
}

#if wxOSX_USE_IPHONE
// Apple has allowed usage of this API as of 15th Dec 2009w
extern CGImageRef UIGetScreenImage();
#endif

// TODO Switch to CGWindowListCreateImage for 10.5 and above

wxBitmap wxScreenDCImpl::DoGetAsBitmap(const wxRect *subrect) const
{
    wxRect rect = subrect ? *subrect : wxRect(0, 0, m_width, m_height);

    wxBitmap bmp(rect.GetSize(), 32);

#if !wxOSX_USE_IPHONE
    CGRect srcRect = CGRectMake(rect.x, rect.y, rect.width, rect.height);

    CGContextRef context = (CGContextRef)bmp.GetHBITMAP();

    CGContextSaveGState(context);

    CGContextTranslateCTM( context, 0,  m_height );
    CGContextScaleCTM( context, 1, -1 );

    if ( subrect )
        srcRect = CGRectOffset( srcRect, -subrect->x, -subrect->y ) ;

    CGImageRef image = NULL;
    
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
    if ( UMAGetSystemVersion() >= 0x1060)
    {
        image = CGDisplayCreateImage(kCGDirectMainDisplay);
    }
    else
#endif
    {
        image = grabViaOpenGL(kCGNullDirectDisplay, srcRect);
    }

    wxASSERT_MSG(image, wxT("wxScreenDC::GetAsBitmap - unable to get screenshot."));

    CGContextDrawImage(context, srcRect, image);

    CGImageRelease(image);

    CGContextRestoreGState(context);
#else
    // TODO implement using UIGetScreenImage, CGImageCreateWithImageInRect, CGContextDrawImage
#endif
    return bmp;
}
