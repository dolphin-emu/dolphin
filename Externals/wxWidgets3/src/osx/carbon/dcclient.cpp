/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dcclient.cpp
// Purpose:     wxClientDCImpl class
// Author:      Stefan Csomor
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/dcclient.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/window.h"
    #include "wx/dcmemory.h"
    #include "wx/settings.h"
    #include "wx/toplevel.h"
    #include "wx/math.h"
    #include "wx/region.h"
#endif

#include "wx/graphics.h"
#include "wx/rawbmp.h"
#include "wx/osx/private.h"
#include "wx/osx/dcclient.h"

//-----------------------------------------------------------------------------
// wxWindowDCImpl
//-----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxWindowDCImpl, wxGCDCImpl);

wxWindowDCImpl::wxWindowDCImpl( wxDC *owner )
   : wxGCDCImpl( owner )
{
    m_release = false;
}


wxWindowDCImpl::wxWindowDCImpl( wxDC *owner, wxWindow *window )
   : wxGCDCImpl( owner )
{
    m_window = window;

    m_ok = true ;

    m_window->GetSize( &m_width , &m_height);
    if ( !m_window->IsShownOnScreen() )
        m_width = m_height = 0;

    CGContextRef cg = (CGContextRef) window->MacGetCGContextRef();

    m_release = false;
    if ( cg == NULL )
    {
        SetGraphicsContext( wxGraphicsContext::Create( window ) ) ;
        m_contentScaleFactor = window->GetContentScaleFactor();
        SetDeviceOrigin(-window->MacGetLeftBorderSize() , -window->MacGetTopBorderSize());
    }
    else
    {
        // determine content scale
        CGRect userrect = CGRectMake(0, 0, 10, 10);
        CGRect devicerect;
        devicerect = CGContextConvertRectToDeviceSpace(cg, userrect);
        m_contentScaleFactor = devicerect.size.height / userrect.size.height;

        CGContextSaveGState( cg );
        m_release = true ;
        // make sure the context is having its origin at the wx-window coordinates of the
        // view (read at the top of window.cpp about the differences)
        if ( window->MacGetLeftBorderSize() != 0 || window->MacGetTopBorderSize() != 0 )
            CGContextTranslateCTM( cg , -window->MacGetLeftBorderSize() , -window->MacGetTopBorderSize() );

        wxGraphicsContext* context = wxGraphicsContext::CreateFromNative( cg );
        context->EnableOffset(m_contentScaleFactor <= 1);
        SetGraphicsContext( context );
    }
    DoSetClippingRegion( 0 , 0 , m_width , m_height ) ;

    SetBackground(wxBrush(window->GetBackgroundColour(),wxBRUSHSTYLE_SOLID));

    SetFont( window->GetFont() ) ;
}

wxWindowDCImpl::~wxWindowDCImpl()
{
    if ( m_release )
    {
        // this must not necessarily be the current context, we must restore the state of the
        // cg we started with above (before the CGContextTranslateCTM call)
        CGContextRef cg = (CGContextRef) m_window->MacGetCGContextRef();
        CGContextRestoreGState(cg);
    }
}

void wxWindowDCImpl::DoGetSize( int* width, int* height ) const
{
    if ( width )
        *width = m_width;
    if ( height )
        *height = m_height;
}

/*
 * wxClientDCImpl
 */

wxIMPLEMENT_ABSTRACT_CLASS(wxClientDCImpl, wxWindowDCImpl);

wxClientDCImpl::wxClientDCImpl( wxDC *owner )
 : wxWindowDCImpl( owner )
{
}

wxClientDCImpl::wxClientDCImpl( wxDC *owner, wxWindow *window ) :
    wxWindowDCImpl( owner, window )
{
    wxCHECK_RET( window, wxT("invalid window in wxClientDCImpl") );
    wxPoint origin = window->GetClientAreaOrigin() ;
    m_window->GetClientSize( &m_width , &m_height);
    if ( !m_window->IsShownOnScreen() )
        m_width = m_height = 0;
    
    int x0,y0;
    DoGetDeviceOrigin(&x0,&y0);
    SetDeviceOrigin( origin.x + x0, origin.y + y0 );
    
    DoSetClippingRegion( 0 , 0 , m_width , m_height ) ;
}

wxClientDCImpl::~wxClientDCImpl()
{
    if( GetGraphicsContext() && GetGraphicsContext()->GetNativeContext() && !m_release )
        Flush();
}

/*
 * wxPaintDCImpl
 */

wxIMPLEMENT_ABSTRACT_CLASS(wxPaintDCImpl, wxWindowDCImpl);

wxPaintDCImpl::wxPaintDCImpl( wxDC *owner )
 : wxWindowDCImpl( owner )
{
}

wxPaintDCImpl::wxPaintDCImpl( wxDC *owner, wxWindow *window ) :
    wxWindowDCImpl( owner, window )
{
    wxASSERT_MSG( window->MacGetCGContextRef() != NULL, wxT("using wxPaintDC without being in a native paint event") );
    wxPoint origin = window->GetClientAreaOrigin() ;
    m_window->GetClientSize( &m_width , &m_height);
    SetDeviceOrigin( origin.x, origin.y );
    DoSetClippingRegion( 0 , 0 , m_width , m_height ) ;
}

wxPaintDCImpl::~wxPaintDCImpl()
{
}
