/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/nonownedwnd.mm
// Purpose:     non owned window for iphone
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-06-20
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/osx/private.h"

#include "wx/nonownedwnd.h"
#include "wx/frame.h"
#include <algorithm>

CGRect wxToNSRect(UIView* parent, const wxRect& r )
{
    CGRect frame = parent ? [parent bounds] : [[UIScreen mainScreen] bounds];
    int y = r.y;
    int x = r.x ;
    return CGRectMake(x, y, r.width , r.height);
}

wxRect wxFromNSRect( UIView* parent, const CGRect& rect )
{
    CGRect frame = parent ? [parent bounds] : [[UIScreen mainScreen] bounds];
    int y = rect.origin.y;
    int x = rect.origin.x;
    return wxRect( x, y, rect.size.width, rect.size.height );
}

CGPoint wxToNSPoint( UIView* parent, const wxPoint& p )
{
    CGRect frame = parent ? [parent bounds] : [[UIScreen mainScreen] bounds];
    int x = p.x ;
    int y = p.y;
    return CGPointMake(x, y);
}

wxPoint wxFromNSPoint( UIView* parent, const CGPoint& p )
{
    CGRect frame = parent ? [parent bounds] : [[UIScreen mainScreen] bounds];
    int x = p.x;
    int y = p.y;
    return wxPoint( x, y);
}

@interface wxUIContentViewController : UIViewController
{
}

@end

@interface wxUIContentView : UIView
{
    wxUIContentViewController* _controller;
}

- (void) setController: (wxUIContentViewController*) controller;
- (wxUIContentViewController*) controller;
@end



//
// c++ impl
//

IMPLEMENT_DYNAMIC_CLASS( wxNonOwnedWindowIPhoneImpl , wxNonOwnedWindowImpl )

wxNonOwnedWindowIPhoneImpl::wxNonOwnedWindowIPhoneImpl( wxNonOwnedWindow* nonownedwnd) :
    wxNonOwnedWindowImpl(nonownedwnd)
{
    m_macWindow = NULL;
    m_macFullScreenData = NULL;
    m_initialShowSent = false;
}

wxNonOwnedWindowIPhoneImpl::wxNonOwnedWindowIPhoneImpl()
{
    m_macWindow = NULL;
    m_macFullScreenData = NULL;
    m_initialShowSent = false;
}

wxNonOwnedWindowIPhoneImpl::~wxNonOwnedWindowIPhoneImpl()
{
    [m_macWindow release];
}

void wxNonOwnedWindowIPhoneImpl::WillBeDestroyed()
{
}

void wxNonOwnedWindowIPhoneImpl::Create( wxWindow* parent, const wxPoint& pos, const wxSize& size,
long style, long extraStyle, const wxString& name )
{
    m_macWindow = [UIWindow alloc];

    UIWindowLevel level = UIWindowLevelNormal;

    // most styles are not supported on the iphone

    if ( style & wxFRAME_TOOL_WINDOW )
    {
        level = UIWindowLevelAlert; ;
    }
    else if ( ( style & wxPOPUP_WINDOW ) )
    {
        level = UIWindowLevelAlert;;
    }
    else if ( ( style & wxCAPTION ) )
    {
    }
    else if ( ( style & wxFRAME_DRAWER ) )
    {
    }
    else
    {
    }

    if ( ( style & wxSTAY_ON_TOP ) )
        level = UIWindowLevelAlert;
    CGRect r = CGRectMake( 0, 0, size.x, size.y) ;
    if ( UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation]) )
        std::swap(r.size.width,r.size.height);

    [m_macWindow initWithFrame:r ];
    [m_macWindow setHidden:YES];

    [m_macWindow setWindowLevel:level];
}

void wxNonOwnedWindowIPhoneImpl::Create( wxWindow* WXUNUSED(parent), WXWindow nativeWindow )
{
    m_macWindow = nativeWindow;
}


WXWindow wxNonOwnedWindowIPhoneImpl::GetWXWindow() const
{
    return m_macWindow;
}

void wxNonOwnedWindowIPhoneImpl::Raise()
{
}

void wxNonOwnedWindowIPhoneImpl::Lower()
{
}

bool wxNonOwnedWindowIPhoneImpl::Show(bool show)
{
    [m_macWindow setHidden:(show ? NO : YES)];
    if ( show )
    {
        if ( !m_initialShowSent )
        {
            wxNonOwnedWindow* now = dynamic_cast<wxNonOwnedWindow*> (GetWXPeer());
            wxShowEvent eventShow(now->GetId(), true);
            eventShow.SetEventObject(now);
            
            now->HandleWindowEvent(eventShow);

            m_initialShowSent = true;
        }
        //[m_macWindow orderFront: self];
        [m_macWindow makeKeyWindow];
    }
    return true;
}

bool wxNonOwnedWindowIPhoneImpl::ShowWithEffect(bool show, wxShowEffect effect, unsigned timeout)
{
    return Show(show);
}

void wxNonOwnedWindowIPhoneImpl::Update()
{
//  TODO  [m_macWindow displayIfNeeded];
}

bool wxNonOwnedWindowIPhoneImpl::SetTransparent(wxByte alpha)
{
    [m_macWindow setAlpha:(CGFloat) alpha/255.0];
    return true;
}

bool wxNonOwnedWindowIPhoneImpl::SetBackgroundColour(const wxColour& col )
{
    return true;
}

void wxNonOwnedWindowIPhoneImpl::SetExtraStyle( long exStyle )
{
    // no special styles supported
}

bool wxNonOwnedWindowIPhoneImpl::SetBackgroundStyle(wxBackgroundStyle style)
{
    return true;
}

bool wxNonOwnedWindowIPhoneImpl::CanSetTransparent()
{
    return true;
}

void wxNonOwnedWindowIPhoneImpl::MoveWindow(int x, int y, int width, int height)
{
    CGRect r = CGRectMake( x,y,width,height) ;
    if ( UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation]) )
        std::swap(r.size.width,r.size.height);

    [m_macWindow setFrame:r];
}

void wxNonOwnedWindowIPhoneImpl::GetPosition( int &x, int &y ) const
{
    CGRect r = [m_macWindow frame];
    x = r.origin.x;
    y = r.origin.y;
}

void wxNonOwnedWindowIPhoneImpl::GetSize( int &width, int &height ) const
{
    CGRect r = [m_macWindow frame];
    if ( UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation]) )
        std::swap(r.size.width,r.size.height);
    width = r.size.width;
    height = r.size.height;
}

void wxNonOwnedWindowIPhoneImpl::GetContentArea( int& left, int &top, int &width, int &height ) const
{
    CGRect r = [m_macWindow bounds];
    if ( UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation]) )
        std::swap(r.size.width,r.size.height);
    width = r.size.width;
    height = r.size.height;
    left = r.origin.x;
    top = r.origin.y;
}

bool wxNonOwnedWindowIPhoneImpl::SetShape(const wxRegion& region)
{
    return false;
}

void wxNonOwnedWindowIPhoneImpl::SetTitle( const wxString& title, wxFontEncoding encoding )
{
// TODO change title of app ?
}

bool wxNonOwnedWindowIPhoneImpl::IsMaximized() const
{
    return false;
}

bool wxNonOwnedWindowIPhoneImpl::IsIconized() const
{
    return false;
}

void wxNonOwnedWindowIPhoneImpl::Iconize( bool iconize )
{
}

void wxNonOwnedWindowIPhoneImpl::Maximize(bool maximize)
{
    if ( maximize )
    {
        CGRect r = [[UIScreen mainScreen] bounds];
        [m_macWindow setFrame:r];
    }
}

bool wxNonOwnedWindowIPhoneImpl::IsFullScreen() const
{
    return m_macFullScreenData != NULL ;
}

bool wxNonOwnedWindowIPhoneImpl::ShowFullScreen(bool show, long style)
{
    return true;
}

void wxNonOwnedWindowIPhoneImpl::RequestUserAttention(int WXUNUSED(flags))
{
}

void wxNonOwnedWindowIPhoneImpl::ScreenToWindow( int *x, int *y )
{
    CGPoint p = CGPointMake(  (x ? *x : 0), (y ? *y : 0) );
    p = [m_macWindow convertPoint:p fromWindow:nil];
    if ( x )
        *x = p.x;
    if ( y )
        *y = p.y;
}

void wxNonOwnedWindowIPhoneImpl::WindowToScreen( int *x, int *y )
{
    CGPoint p = CGPointMake(  (x ? *x : 0), (y ? *y : 0) );
    p = [m_macWindow convertPoint:p toWindow:nil];
    if ( x )
        *x = p.x;
    if ( y )
        *y = p.y;
}

wxNonOwnedWindowImpl* wxNonOwnedWindowImpl::CreateNonOwnedWindow( wxNonOwnedWindow* wxpeer, wxWindow* parent, WXWindow nativeWindow)
{
    wxNonOwnedWindowIPhoneImpl* now = new wxNonOwnedWindowIPhoneImpl( wxpeer );
    now->Create( parent, nativeWindow );
    return now;
}


wxNonOwnedWindowImpl* wxNonOwnedWindowImpl::CreateNonOwnedWindow( wxNonOwnedWindow* wxpeer, wxWindow* parent, const wxPoint& pos, const wxSize& size,
    long style, long extraStyle, const wxString& name )
{
    wxNonOwnedWindowImpl* now = new wxNonOwnedWindowIPhoneImpl( wxpeer );
    now->Create( parent, pos, size, style , extraStyle, name );
    return now;
}

wxWidgetImpl* wxWidgetImpl::CreateContentView( wxNonOwnedWindow* now )
{
    UIWindow* toplevelwindow = now->GetWXWindow();
    CGRect frame = [toplevelwindow bounds];
    CGRect appframe = [[UIScreen mainScreen] applicationFrame];
    BOOL fullscreen = now->GetWindowStyle() == wxDEFAULT_FRAME_STYLE && [[UIApplication sharedApplication] statusBarStyle] == UIStatusBarStyleBlackTranslucent;

    wxUIContentView* contentview = [[wxUIContentView alloc] initWithFrame:( fullscreen ? frame : appframe ) ];
    contentview.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    wxUIContentViewController* controller = [[wxUIContentViewController alloc] initWithNibName:nil bundle:nil];

#ifdef __IPHONE_3_0
    controller.wantsFullScreenLayout = fullscreen;
#endif

    controller.view = contentview;
    [contentview release];
    [contentview setController:controller];
    [contentview setHidden:YES];
    
    wxWidgetIPhoneImpl* impl = new wxWidgetIPhoneImpl( now, contentview, true );
    impl->InstallEventHandler();
    
    if ([toplevelwindow respondsToSelector:@selector(setRootViewController:)])
    {
        toplevelwindow.rootViewController = controller;
    }
    else
    {
        [toplevelwindow addSubview:contentview];
    }
    return impl;
}

//
// obj-c impl
//

@implementation wxUIContentView

- (void) setController: (wxUIContentViewController*) controller
{
    _controller = controller;
}

- (wxUIContentViewController*) controller
{
    return _controller;
}

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}
 
@end

@implementation wxUIContentViewController

- (BOOL) shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation) interfaceOrientation
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( [self view] );
    wxNonOwnedWindow* now = dynamic_cast<wxNonOwnedWindow*> (impl->GetWXPeer());
    
    // TODO: determine NO or YES based on min size requirements (whether it fits on the new orientation)
    
    return YES;
}

// iOS 6 support, right now unconditionally supporting all orientations, TODO use a orientation mask

-(BOOL)shouldAutorotate
{
    return YES;
}

 - (NSUInteger)supportedInterfaceOrientations
{
     return UIInterfaceOrientationMaskAll;
}
 
 


- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( [self view] );
    wxNonOwnedWindow* now = dynamic_cast<wxNonOwnedWindow*> (impl->GetWXPeer());
    
    now->HandleResized(0);
}

-(void) viewWillAppear:(BOOL)animated
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( [self view] );
    wxNonOwnedWindow* now = dynamic_cast<wxNonOwnedWindow*> (impl->GetWXPeer());
    wxNonOwnedWindowIPhoneImpl* nowimpl = dynamic_cast<wxNonOwnedWindowIPhoneImpl*> (now->GetNonOwnedPeer());
    
    if ( nowimpl->InitialShowEventSent() )
    {
        wxShowEvent eventShow(now->GetId(), true);
        eventShow.SetEventObject(now);
    
        now->HandleWindowEvent(eventShow);
    }
}

-(void) viewWillDisappear:(BOOL)animated
{
    wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( [self view] );
    if( impl )
    {
        wxNonOwnedWindow* now = dynamic_cast<wxNonOwnedWindow*> (impl->GetWXPeer());
        wxNonOwnedWindowIPhoneImpl* nowimpl = dynamic_cast<wxNonOwnedWindowIPhoneImpl*> (now->GetNonOwnedPeer());
        
        if ( nowimpl->InitialShowEventSent() )
        {
            wxShowEvent eventShow(now->GetId(), false);
            eventShow.SetEventObject(now);
        
            now->HandleWindowEvent(eventShow);
        }
    }
}

-(void) dealloc
{
    [super dealloc];
}

- (UIView*) rotatingFooterView
{
    UIView* footerView = [super rotatingFooterView];
    if ( footerView == nil )
    {
        wxWidgetIPhoneImpl* impl = (wxWidgetIPhoneImpl* ) wxWidgetImpl::FindFromWXWidget( [self view] );
        wxFrame* frame = dynamic_cast<wxFrame*> (impl->GetWXPeer());
        if ( frame && frame->GetToolBar())
        {
            footerView = frame->GetToolBar()->GetHandle();
        }
    }
    return footerView;
}

@end



