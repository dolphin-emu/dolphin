/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/nonownedwnd_osx.cpp
// Purpose:     implementation of wxNonOwnedWindow
// Author:      Stefan Csomor
// Created:     2008-03-24
// Copyright:   (c) Stefan Csomor 2008
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/dcmemory.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/hashmap.h"
#include "wx/evtloop.h"
#include "wx/tooltip.h"
#include "wx/nonownedwnd.h"

#include "wx/osx/private.h"
#include "wx/settings.h"
#include "wx/frame.h"

#if wxUSE_SYSTEM_OPTIONS
    #include "wx/sysopt.h"
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// trace mask for activation tracing messages
#define TRACE_ACTIVATE "activation"

wxWindow* g_MacLastWindow = NULL ;

clock_t wxNonOwnedWindow::s_lastFlush = 0;

// unified title and toolbar constant - not in Tiger headers, so we duplicate it here
#define kWindowUnifiedTitleAndToolbarAttribute (1 << 7)

// ---------------------------------------------------------------------------
// wxWindowMac utility functions
// ---------------------------------------------------------------------------

WX_DECLARE_HASH_MAP(WXWindow, wxNonOwnedWindowImpl*, wxPointerHash, wxPointerEqual, MacWindowMap);

static MacWindowMap wxWinMacWindowList;

wxNonOwnedWindow* wxNonOwnedWindow::GetFromWXWindow( WXWindow win )
{
    wxNonOwnedWindowImpl* impl = wxNonOwnedWindowImpl::FindFromWXWindow(win);

    return ( impl != NULL ? impl->GetWXPeer() : NULL ) ;
}

wxNonOwnedWindowImpl* wxNonOwnedWindowImpl::FindFromWXWindow (WXWindow window)
{
    MacWindowMap::iterator node = wxWinMacWindowList.find(window);

    return (node == wxWinMacWindowList.end()) ? NULL : node->second;
}

void wxNonOwnedWindowImpl::RemoveAssociations( wxNonOwnedWindowImpl* impl)
{
    MacWindowMap::iterator it;
    for ( it = wxWinMacWindowList.begin(); it != wxWinMacWindowList.end(); ++it )
    {
        if ( it->second == impl )
        {
            wxWinMacWindowList.erase(it);
            break;
        }
    }
}

void wxNonOwnedWindowImpl::Associate( WXWindow window, wxNonOwnedWindowImpl *impl )
{
    // adding NULL WindowRef is (first) surely a result of an error and
    // nothing else :-)
    wxCHECK_RET( window != (WXWindow) NULL, wxT("attempt to add a NULL WindowRef to window list") );

    wxWinMacWindowList[window] = impl;
}

// ----------------------------------------------------------------------------
// wxNonOwnedWindow creation
// ----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxNonOwnedWindowImpl, wxObject);

wxNonOwnedWindow *wxNonOwnedWindow::s_macDeactivateWindow = NULL;

void wxNonOwnedWindow::Init()
{
    m_nowpeer = NULL;
    m_isNativeWindowWrapper = false;
}

bool wxNonOwnedWindow::Create(wxWindow *parent,
                                 wxWindowID id,
                                 const wxPoint& posOrig,
                                 const wxSize& sizeOrig,
                                 long style,
                                 const wxString& name)
{
    m_windowStyle = style;

    SetName( name );

    m_windowId = id == -1 ? NewControlId() : id;
    m_windowStyle = style;
    m_isShown = false;

    // use the appropriate defaults for the position and size if necessary
    wxSize size(sizeOrig);
    if ( !size.IsFullySpecified() )
        size.SetDefaults(wxTopLevelWindow::GetDefaultSize());

    wxPoint pos(posOrig);
    if ( !pos.IsFullySpecified() )
    {
        wxRect rectWin = wxRect(size).CentreIn(wxGetClientDisplayRect());

        // The size of the window is often not really known yet, TLWs are often
        // created with some small initial size and later are fitted to contain
        // their children so centering the window will show it too much to the
        // right and bottom, adjust for it by putting it more to the left and
        // center.
        rectWin.x /= 2;
        rectWin.y /= 2;

        pos.SetDefaults(rectWin.GetPosition());
    }

    // create frame.
    m_nowpeer = wxNonOwnedWindowImpl::CreateNonOwnedWindow
                (
                    this, parent,
                    pos , size,
                    style, GetExtraStyle(),
                    name
                );
    wxNonOwnedWindowImpl::Associate( m_nowpeer->GetWXWindow() , m_nowpeer ) ;
    SetPeer(wxWidgetImpl::CreateContentView(this));

    DoSetWindowVariant( m_windowVariant ) ;

    wxWindowCreateEvent event(this);
    HandleWindowEvent(event);

    SetBackgroundColour(wxSystemSettings::GetColour( wxSYS_COLOUR_3DFACE ));

    if ( parent )
        parent->AddChild(this);

    return true;
}

bool wxNonOwnedWindow::Create(wxWindow *parent, WXWindow nativeWindow)
{
    if ( parent )
        parent->AddChild(this);

    SubclassWin(nativeWindow);

    return true;
}

void wxNonOwnedWindow::SubclassWin(WXWindow nativeWindow)
{
    wxASSERT_MSG( !m_isNativeWindowWrapper, wxT("subclassing window twice?") );
    wxASSERT_MSG( m_nowpeer == NULL, wxT("window already was created") );

    m_nowpeer = wxNonOwnedWindowImpl::CreateNonOwnedWindow(this, GetParent(), nativeWindow );
    m_isNativeWindowWrapper = true;
    wxNonOwnedWindowImpl::Associate( m_nowpeer->GetWXWindow() , m_nowpeer ) ;
    SetPeer(wxWidgetImpl::CreateContentView(this));
}

void wxNonOwnedWindow::UnsubclassWin()
{
    wxASSERT_MSG( m_isNativeWindowWrapper, wxT("window was not subclassed") );

    if ( GetParent() )
        GetParent()->RemoveChild(this);

    wxNonOwnedWindowImpl::RemoveAssociations(m_nowpeer) ;
    wxDELETE(m_nowpeer);
    SetPeer(NULL);
    m_isNativeWindowWrapper = false;
}


wxNonOwnedWindow::~wxNonOwnedWindow()
{
    SendDestroyEvent();

    wxNonOwnedWindowImpl::RemoveAssociations(m_nowpeer) ;

    DestroyChildren();

    wxDELETE(m_nowpeer);

    // avoid dangling refs
    if ( s_macDeactivateWindow == this )
        s_macDeactivateWindow = NULL;
}

bool wxNonOwnedWindow::Destroy()
{
    WillBeDestroyed();

    return wxWindow::Destroy();
}

void wxNonOwnedWindow::WillBeDestroyed()
{
    if ( m_nowpeer )
        m_nowpeer->WillBeDestroyed();
}

// ----------------------------------------------------------------------------
// wxNonOwnedWindow misc
// ----------------------------------------------------------------------------

bool wxNonOwnedWindow::OSXShowWithEffect(bool show,
                                         wxShowEffect effect,
                                         unsigned timeout)
{
    if ( effect == wxSHOW_EFFECT_NONE ||
            !m_nowpeer || !m_nowpeer->ShowWithEffect(show, effect, timeout) )
        return Show(show);

    if ( show )
    {
        // as apps expect a size event to occur when the window is shown,
        // generate one when it is shown with effect too
        SendSizeEvent();
    }

    return true;
}

wxPoint wxNonOwnedWindow::GetClientAreaOrigin() const
{
    int left, top, width, height;
    m_nowpeer->GetContentArea(left, top, width, height);
    return wxPoint(left, top);
}

bool wxNonOwnedWindow::SetBackgroundColour(const wxColour& c )
{
    if ( !wxWindow::SetBackgroundColour(c) && m_hasBgCol )
        return false ;

    if ( GetBackgroundStyle() != wxBG_STYLE_CUSTOM )
    {
        if ( m_nowpeer )
            return m_nowpeer->SetBackgroundColour(c);
    }
    return true;
}

void wxNonOwnedWindow::SetWindowStyleFlag(long flags)
{
    if (flags == GetWindowStyleFlag())
        return;

    wxWindow::SetWindowStyleFlag(flags);

    if (m_nowpeer)
        m_nowpeer->SetWindowStyleFlag(flags);
}

// Raise the window to the top of the Z order
void wxNonOwnedWindow::Raise()
{
    m_nowpeer->Raise();
}

// Lower the window to the bottom of the Z order
void wxNonOwnedWindow::Lower()
{
    m_nowpeer->Lower();
}

void wxNonOwnedWindow::HandleActivated( double timestampsec, bool didActivate )
{
    MacActivate( (int) (timestampsec * 1000) , didActivate);
    wxActivateEvent wxevent(wxEVT_ACTIVATE, didActivate , GetId());
    wxevent.SetTimestamp( (int) (timestampsec * 1000) );
    wxevent.SetEventObject(this);
    HandleWindowEvent(wxevent);
}

void wxNonOwnedWindow::HandleResized( double WXUNUSED(timestampsec) )
{
    SendSizeEvent();
    // we have to inform some controls that have to reset things
    // relative to the toplevel window (e.g. OpenGL buffers)
    wxWindowMac::MacSuperChangedPosition() ; // like this only children will be notified
}

void wxNonOwnedWindow::HandleResizing( double WXUNUSED(timestampsec), wxRect* rect )
{
    wxRect r = *rect ;

    // this is a EVT_SIZING not a EVT_SIZE type !
    wxSizeEvent wxevent( r , GetId() ) ;
    wxevent.SetEventObject( this ) ;
    if ( HandleWindowEvent(wxevent) )
        r = wxevent.GetRect() ;

    if ( GetMaxWidth() != -1 && r.GetWidth() > GetMaxWidth() )
        r.SetWidth( GetMaxWidth() ) ;
    if ( GetMaxHeight() != -1 && r.GetHeight() > GetMaxHeight() )
        r.SetHeight( GetMaxHeight() ) ;
    if ( GetMinWidth() != -1 && r.GetWidth() < GetMinWidth() )
        r.SetWidth( GetMinWidth() ) ;
    if ( GetMinHeight() != -1 && r.GetHeight() < GetMinHeight() )
        r.SetHeight( GetMinHeight() ) ;

    *rect = r;
    // TODO actuall this is too early, in case the window extents are adjusted
    wxWindowMac::MacSuperChangedPosition() ; // like this only children will be notified
}

void wxNonOwnedWindow::HandleMoved( double timestampsec )
{
    wxMoveEvent wxevent( GetPosition() , GetId());
    wxevent.SetTimestamp( (int) (timestampsec * 1000) );
    wxevent.SetEventObject( this );
    HandleWindowEvent(wxevent);
}

void wxNonOwnedWindow::MacDelayedDeactivation(long timestamp)
{
    if (s_macDeactivateWindow)
    {
        wxLogTrace(TRACE_ACTIVATE,
                   wxT("Doing delayed deactivation of %p"),
                   s_macDeactivateWindow);

        s_macDeactivateWindow->MacActivate(timestamp, false);
    }
}

void wxNonOwnedWindow::MacActivate( long timestamp , bool WXUNUSED(inIsActivating) )
{
    wxLogTrace(TRACE_ACTIVATE, wxT("TopLevel=%p::MacActivate"), this);

    if (s_macDeactivateWindow == this)
        s_macDeactivateWindow = NULL;

    MacDelayedDeactivation(timestamp);
}

bool wxNonOwnedWindow::Show(bool show)
{
    if ( !wxWindow::Show(show) )
        return false;

    if ( m_nowpeer )
        m_nowpeer->Show(show);

    if ( show )
    {
        // because apps expect a size event to occur at this moment
        SendSizeEvent();
    }

    return true ;
}

bool wxNonOwnedWindow::SetTransparent(wxByte alpha)
{
    return m_nowpeer->SetTransparent(alpha);
}


bool wxNonOwnedWindow::CanSetTransparent()
{
    return m_nowpeer->CanSetTransparent();
}


void wxNonOwnedWindow::SetExtraStyle(long exStyle)
{
    if ( GetExtraStyle() == exStyle )
        return ;

    wxWindow::SetExtraStyle( exStyle ) ;

    if ( m_nowpeer )
        m_nowpeer->SetExtraStyle(exStyle);
}

bool wxNonOwnedWindow::SetBackgroundStyle(wxBackgroundStyle style)
{
    if ( !wxWindow::SetBackgroundStyle(style) )
        return false ;

    return m_nowpeer ? m_nowpeer->SetBackgroundStyle(style) : true;
}

void wxNonOwnedWindow::DoMoveWindow(int x, int y, int width, int height)
{
    if ( m_nowpeer == NULL )
        return;

    m_cachedClippedRectValid = false ;

    m_nowpeer->MoveWindow(x, y, width, height);
    wxWindowMac::MacSuperChangedPosition() ; // like this only children will be notified
}

void wxNonOwnedWindow::DoGetPosition( int *x, int *y ) const
{
    if ( m_nowpeer == NULL )
        return;

    int x1,y1 ;
    m_nowpeer->GetPosition(x1, y1);

    if (x)
       *x = x1 ;
    if (y)
       *y = y1 ;
}

void wxNonOwnedWindow::DoGetSize( int *width, int *height ) const
{
    if ( m_nowpeer == NULL )
        return;

    int w,h;

    m_nowpeer->GetSize(w, h);

    if (width)
       *width = w ;
    if (height)
       *height = h ;
}

void wxNonOwnedWindow::DoGetClientSize( int *width, int *height ) const
{
    if ( m_nowpeer == NULL )
        return;

    int left, top, w, h;
    // under iphone with a translucent status bar the m_nowpeer returns the
    // inner area, while the content area extends under the translucent
    // status bar, therefore we use the content view's area
#ifdef __WXOSX_IPHONE__
    GetPeer()->GetContentArea(left, top, w, h);
#else
    m_nowpeer->GetContentArea(left, top, w, h);
#endif
    
    if (width)
       *width = w ;
    if (height)
       *height = h ;
}

void wxNonOwnedWindow::WindowWasPainted()
{
    s_lastFlush = clock();
}

void wxNonOwnedWindow::Update()
{
    if ( clock() - s_lastFlush > CLOCKS_PER_SEC / 30 )
    {
        s_lastFlush = clock();
        m_nowpeer->Update();
    }
}

WXWindow wxNonOwnedWindow::GetWXWindow() const
{
    return m_nowpeer ? m_nowpeer->GetWXWindow() : NULL;
}

#if wxOSX_USE_COCOA_OR_IPHONE
void *wxNonOwnedWindow::OSXGetViewOrWindow() const
{
    return GetWXWindow();
}
#endif

// ---------------------------------------------------------------------------
// Shape implementation
// ---------------------------------------------------------------------------

bool wxNonOwnedWindow::DoClearShape()
{
    m_shape.Clear();

    wxSize sz = GetClientSize();
    wxRegion region(0, 0, sz.x, sz.y);

    return m_nowpeer->SetShape(region);
}

bool wxNonOwnedWindow::DoSetRegionShape(const wxRegion& region)
{
    m_shape = region;

    return m_nowpeer->SetShape(region);
}

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/scopedptr.h"

bool wxNonOwnedWindow::DoSetPathShape(const wxGraphicsPath& path)
{
    m_shapePath = path;

    // Convert the path to wxRegion by rendering the path on a window-sized
    // bitmap, creating a mask from it and finally creating the region from
    // this mask.
    wxBitmap bmp(GetSize());

    {
        wxMemoryDC dc(bmp);
        dc.SetBackground(*wxBLACK);
        dc.Clear();

        wxScopedPtr<wxGraphicsContext> context(wxGraphicsContext::Create(dc));
        context->SetBrush(*wxWHITE);
        context->FillPath(m_shapePath);
    }

    bmp.SetMask(new wxMask(bmp, *wxBLACK));

    return DoSetRegionShape(wxRegion(bmp));
}

void
wxNonOwnedWindow::OSXHandleMiniaturize(double WXUNUSED(timestampsec),
                                       bool miniaturized)
{
    if ( wxTopLevelWindowMac* top = (wxTopLevelWindowMac*) MacGetTopLevelWindow() )
        top->OSXSetIconizeState(miniaturized);
}

#endif // wxUSE_GRAPHICS_CONTEXT
