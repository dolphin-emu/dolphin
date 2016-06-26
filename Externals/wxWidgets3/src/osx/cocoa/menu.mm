/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/menu.mm
// Purpose:     wxMenu, wxMenuBar, wxMenuItem
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// headers & declarations
// ============================================================================

// wxWidgets headers
// -----------------

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/log.h"
#include "wx/app.h"
#include "wx/utils.h"
#include "wx/frame.h"
#include "wx/menuitem.h"
#endif

#include "wx/menu.h"

#include "wx/osx/private.h"

// other standard headers
// ----------------------
#include <string.h>

@implementation wxNSMenu

- (id) initWithTitle:(NSString*) title
{
    self = [super initWithTitle:title];
    impl = NULL;
    return self;
}

- (void)setImplementation: (wxMenuImpl *) theImplementation
{
    impl = theImplementation;
}

- (wxMenuImpl*) implementation
{
    return impl;
}

@end

// this is more compatible, as it is also called for command-key shortcuts
// and under 10.4, we are not getting a 'close' event however...
#define wxOSX_USE_NEEDSUPDATE_HOOK 1

@interface wxNSMenuController : NSObject <NSMenuDelegate>
{
}

#if wxOSX_USE_NEEDSUPDATE_HOOK
- (void)menuNeedsUpdate:(NSMenu*)smenu;
#else
- (void)menuWillOpen:(NSMenu *)menu;
#endif
- (void)menuDidClose:(NSMenu *)menu;
- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item;

@end

@implementation wxNSMenuController

- (id) init
{
    self = [super init];
    return self;
}

#if wxOSX_USE_NEEDSUPDATE_HOOK
- (void)menuNeedsUpdate:(NSMenu*)smenu
{
    wxNSMenu* menu = (wxNSMenu*) smenu;
    wxMenuImpl* menuimpl = [menu implementation];
    if ( menuimpl )
    {
        wxMenu* wxpeer = (wxMenu*) menuimpl->GetWXPeer();
        if ( wxpeer )
            wxpeer->HandleMenuOpened();
    }
}
#else
- (void)menuWillOpen:(NSMenu *)smenu
{
    wxNSMenu* menu = (wxNSMenu*) smenu;
    wxMenuImpl* menuimpl = [menu implementation];
    if ( menuimpl )
    {
        wxMenu* wxpeer = (wxMenu*) menuimpl->GetWXPeer();
        if ( wxpeer )
            wxpeer->HandleMenuOpened();
    }
}
#endif

- (void)menuDidClose:(NSMenu *)smenu
{
    wxNSMenu* menu = (wxNSMenu*) smenu;
    wxMenuImpl* menuimpl = [menu implementation];
    if ( menuimpl )
    {
        wxMenu* wxpeer = (wxMenu*) menuimpl->GetWXPeer();
        if ( wxpeer )
            wxpeer->HandleMenuClosed();
    }
}

- (void)menu:(NSMenu *)smenu willHighlightItem:(NSMenuItem *)item
{
    wxNSMenu* menu = (wxNSMenu*) smenu;
    wxMenuImpl* menuimpl = [menu implementation];
    if ( menuimpl )
    {
        wxMenu* wxpeer = (wxMenu*) menuimpl->GetWXPeer();
        if ( [ item isKindOfClass:[wxNSMenuItem class] ] )
        {
            wxMenuItemImpl* menuitemimpl = (wxMenuItemImpl*) [ (wxNSMenuItem*) item implementation ];
            if ( wxpeer && menuitemimpl )
            {
                wxpeer->HandleMenuItemHighlighted( menuitemimpl->GetWXPeer() );
            }
        }
    }
}

@end

@interface NSApplication(MissingAppleMenuCall)
- (void)setAppleMenu:(NSMenu *)menu;
@end

class wxMenuCocoaImpl : public wxMenuImpl
{
public :
    wxMenuCocoaImpl( wxMenu* peer , wxNSMenu* menu) : wxMenuImpl(peer), m_osxMenu(menu)
    {
        static wxNSMenuController* controller = NULL;
        if ( controller == NULL )
        {
            controller = [[wxNSMenuController alloc] init];
        }
        [menu setDelegate:controller];
        [m_osxMenu setImplementation:this];
        // gc aware
        if ( m_osxMenu )
            CFRetain(m_osxMenu);
        [m_osxMenu release];
    }

    virtual ~wxMenuCocoaImpl();

    virtual void InsertOrAppend(wxMenuItem *pItem, size_t pos) wxOVERRIDE
    {
        NSMenuItem* nsmenuitem = (NSMenuItem*) pItem->GetPeer()->GetHMenuItem();
        // make sure a call of SetSubMenu is also reflected (occurring after Create)
        // update the native menu item accordingly
        
        if ( pItem->IsSubMenu() )
        {
            wxMenu* wxsubmenu = pItem->GetSubMenu();
            WXHMENU nssubmenu = wxsubmenu->GetHMenu();
            if ( [nsmenuitem submenu] != nssubmenu )
            {
                wxsubmenu->GetPeer()->SetTitle( pItem->GetItemLabelText() );
                [nsmenuitem setSubmenu:nssubmenu];
            }
        }
        
        if ( pos == (size_t) -1 )
            [m_osxMenu addItem:nsmenuitem ];
        else
            [m_osxMenu insertItem:nsmenuitem atIndex:pos];
    }

    virtual void Remove( wxMenuItem *pItem ) wxOVERRIDE
    {
        [m_osxMenu removeItem:(NSMenuItem*) pItem->GetPeer()->GetHMenuItem()];
    }

    virtual void MakeRoot() wxOVERRIDE
    {
        wxMenu* peer = GetWXPeer();
        
        [NSApp setMainMenu:m_osxMenu];
        [NSApp setAppleMenu:[[m_osxMenu itemAtIndex:0] submenu]];

        wxMenuItem *services = peer->FindItem(wxID_OSX_SERVICES);
        if ( services )
            [NSApp setServicesMenu:services->GetSubMenu()->GetHMenu()];
#if 0
        // should we reset this just to be sure we don't leave a dangling ref ?
        else
            [NSApp setServicesMenu:nil];
#endif
        
        NSMenu* helpMenu = nil;
        int helpid = peer->FindItem(wxApp::s_macHelpMenuTitleName);
        if ( helpid == wxNOT_FOUND )
            helpid = peer->FindItem(_("&Help"));
        
        if ( helpid != wxNOT_FOUND )
        {
            wxMenuItem* helpMenuItem = peer->FindItem(helpid);
            
            if ( helpMenuItem->IsSubMenu() )
                helpMenu = helpMenuItem->GetSubMenu()->GetHMenu();
        }
        if ( [NSApp respondsToSelector:@selector(setHelpMenu:)])
            [NSApp setHelpMenu:helpMenu];
        
    }

    virtual void Enable( bool WXUNUSED(enable) )
    {
    }

    virtual void SetTitle( const wxString& text ) wxOVERRIDE
    {
        wxCFStringRef cfText(text);
        [m_osxMenu setTitle:cfText.AsNSString()];
    }

    virtual void PopUp( wxWindow *win, int x, int y ) wxOVERRIDE
    {
        win->ScreenToClient( &x , &y ) ;
        NSView *view = win->GetPeer()->GetWXWidget();
        [m_osxMenu popUpMenuPositioningItem:nil atLocation:NSMakePoint(x, y) inView:view];
    }

    virtual void GetMenuBarDimensions(int &x, int &y, int &width, int &height) const wxOVERRIDE
    {
        NSRect r = [(NSScreen*)[[NSScreen screens] objectAtIndex:0] frame];
        height = [m_osxMenu menuBarHeight];
        x = r.origin.x;
        y = r.origin.y;
        width = r.size.width;
    }

    WXHMENU GetHMenu() wxOVERRIDE { return m_osxMenu; }

    static wxMenuImpl* Create( wxMenu* peer, const wxString& title );
    static wxMenuImpl* CreateRootMenu( wxMenu* peer );
protected :
    wxNSMenu* m_osxMenu;
} ;

wxMenuCocoaImpl::~wxMenuCocoaImpl()
{
    [m_osxMenu setDelegate:nil];
    [m_osxMenu setImplementation:nil];
    // gc aware
    if ( m_osxMenu )
        CFRelease(m_osxMenu);
}

wxMenuImpl* wxMenuImpl::Create( wxMenu* peer, const wxString& title )
{
    wxCFStringRef cfText( title );
    wxNSMenu* menu = [[wxNSMenu alloc] initWithTitle:cfText.AsNSString()];
    wxMenuImpl* c = new wxMenuCocoaImpl( peer, menu );
    return c;
}
