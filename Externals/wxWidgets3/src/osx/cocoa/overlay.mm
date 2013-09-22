/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/overlay.mm
// Purpose:     common wxOverlay code
// Author:      Stefan Csomor
// Modified by:
// Created:     2006-10-20
// Copyright:   (c) wxWidgets team
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

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/overlay.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
#endif

#include "wx/dcgraph.h"

#include "wx/private/overlay.h"

#ifdef wxHAS_NATIVE_OVERLAY

// ============================================================================
// implementation
// ============================================================================

wxOverlayImpl::wxOverlayImpl()
{
    m_window = NULL ;
    m_overlayContext = NULL ;
    m_overlayWindow = NULL ;
}

wxOverlayImpl::~wxOverlayImpl()
{
    Reset();
}

bool wxOverlayImpl::IsOk()
{
    return m_overlayWindow != NULL ;
}

void wxOverlayImpl::CreateOverlayWindow()
{
    if ( m_window )
    {
        m_overlayParentWindow = m_window->MacGetTopLevelWindowRef();
        [m_overlayParentWindow makeKeyAndOrderFront:nil];
        
        NSView* view = m_window->GetHandle();

        NSPoint viewOriginBase, viewOriginScreen;
        viewOriginBase = [view convertPoint:NSMakePoint(0, 0) toView:nil];
        viewOriginScreen = [m_overlayParentWindow convertBaseToScreen:viewOriginBase];
        
        NSSize viewSize = [view frame].size;
        if ( [view isFlipped] )
            viewOriginScreen.y -= viewSize.height;
        
        m_overlayWindow=[[NSWindow alloc] initWithContentRect:NSMakeRect(viewOriginScreen.x,viewOriginScreen.y,
                                                                         viewSize.width,
                                                                         viewSize.height) 
                                                    styleMask:NSBorderlessWindowMask 
                                                      backing:NSBackingStoreBuffered 
                                                        defer:YES];
        
        [m_overlayParentWindow addChildWindow:m_overlayWindow ordered:NSWindowAbove];
    }
    else
    {
        m_overlayParentWindow = NULL ;
        CGRect cgbounds ;
        cgbounds = CGDisplayBounds(CGMainDisplayID());
 
        m_overlayWindow=[[NSWindow alloc] initWithContentRect:NSMakeRect(cgbounds.origin.x,cgbounds.origin.y,
                                                                       cgbounds.size.width,
                                                                       cgbounds.size.height) 
                                                  styleMask:NSBorderlessWindowMask 
                                                    backing:NSBackingStoreBuffered 
                                                      defer:YES];
    }
    [m_overlayWindow setOpaque:NO];
    [m_overlayWindow setIgnoresMouseEvents:YES];
    [m_overlayWindow setAlphaValue:1.0];
    
    [m_overlayWindow orderFront:nil];
}

void wxOverlayImpl::Init( wxDC* dc, int x , int y , int width , int height )
{
    wxASSERT_MSG( !IsOk() , _("You cannot Init an overlay twice") );

    m_window = dc->GetWindow();
    m_x = x ;
    m_y = y ;
    if ( dc->IsKindOf( CLASSINFO( wxClientDC ) ))
    {
        wxPoint origin = m_window->GetClientAreaOrigin();
        m_x += origin.x;
        m_y += origin.y;
    }
    m_width = width ;
    m_height = height ;

    CreateOverlayWindow();
    wxASSERT_MSG(  m_overlayWindow != NULL , _("Couldn't create the overlay window") );
    m_overlayContext = (CGContextRef) [[m_overlayWindow graphicsContext] graphicsPort];
    wxASSERT_MSG(  m_overlayContext != NULL , _("Couldn't init the context on the overlay window") );

    int ySize = 0;
    if ( m_window )
    {
        NSView* view = m_window->GetHandle();    
        NSSize viewSize = [view frame].size;
        ySize = viewSize.height;
    }
    else
    {
        CGRect cgbounds ;
        cgbounds = CGDisplayBounds(CGMainDisplayID());
        ySize = cgbounds.size.height;
        
        
        
    }
    CGContextTranslateCTM( m_overlayContext, 0, ySize );
    CGContextScaleCTM( m_overlayContext, 1, -1 );
    CGContextTranslateCTM( m_overlayContext, -m_x , -m_y );
}

void wxOverlayImpl::BeginDrawing( wxDC* dc)
{
    wxDCImpl *impl = dc->GetImpl();
    wxGCDCImpl *win_impl = wxDynamicCast(impl,wxGCDCImpl);
    if (win_impl)
    {
        win_impl->SetGraphicsContext( wxGraphicsContext::CreateFromNative( m_overlayContext ) );
        dc->SetClippingRegion( m_x , m_y , m_width , m_height ) ;
    }
}

void wxOverlayImpl::EndDrawing( wxDC* dc)
{
    wxDCImpl *impl = dc->GetImpl();
    wxGCDCImpl *win_impl = wxDynamicCast(impl,wxGCDCImpl);
    if (win_impl)
        win_impl->SetGraphicsContext(NULL);

    CGContextFlush( m_overlayContext );
}

void wxOverlayImpl::Clear(wxDC* WXUNUSED(dc))
{
    wxASSERT_MSG( IsOk() , _("You cannot Clear an overlay that is not inited") );
    CGRect box  = CGRectMake( m_x - 1, m_y - 1 , m_width + 2 , m_height + 2 );
    CGContextClearRect( m_overlayContext, box );
}

void wxOverlayImpl::Reset()
{
    if ( m_overlayContext )
    {
        m_overlayContext = NULL ;
    }

    // todo : don't dispose, only hide and reposition on next run
    if (m_overlayWindow)
    {
        [m_overlayParentWindow removeChildWindow:m_overlayWindow];
        [m_overlayWindow release];
        m_overlayWindow = NULL ;
    }
}

#endif // wxHAS_NATIVE_OVERLAY
