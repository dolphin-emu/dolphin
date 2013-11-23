///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/menuitem.mm
// Purpose:     wxMenuItem implementation
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/menuitem.h"
#include "wx/stockitem.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/menu.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

// a mapping from wx ids to standard osx actions in order to support the native menu item handling
// if a new mapping is added, make sure the wxNonOwnedWindowController has a handler for this action as well

struct Mapping
{
    int menuid;
    SEL action;
};

Mapping sActionToWXMapping[] =
{
// as we don't have NSUndoManager support we must not use the native actions
#if 0
    { wxID_UNDO, @selector(undo:) },
    { wxID_REDO, @selector(redo:) },
#endif
    { wxID_CUT, @selector(cut:) },
    { wxID_COPY, @selector(copy:) },
    { wxID_PASTE, @selector(paste:) },
    { wxID_CLEAR, @selector(delete:) },
    { wxID_SELECTALL, @selector(selectAll:) },
    { 0, NULL }
};

int wxOSXGetIdFromSelector(SEL action )
{
    int i = 0 ;
    while ( sActionToWXMapping[i].action != nil )
    {
        if ( sActionToWXMapping[i].action == action )
            return sActionToWXMapping[i].menuid;
        ++i;
    }
    
    return 0;
}

SEL wxOSXGetSelectorFromID(int menuId )
{
    int i = 0 ;
    while ( sActionToWXMapping[i].action != nil )
    {
        if ( sActionToWXMapping[i].menuid == menuId )
            return sActionToWXMapping[i].action;
        ++i;
    }
    
    return nil;
}


@implementation wxNSMenuItem

- (id) initWithTitle:(NSString *)aString action:(SEL)aSelector keyEquivalent:(NSString *)charCode
{
    self = [super initWithTitle:aString action:aSelector keyEquivalent:charCode];
    return self;
}

- (void) clickedAction: (id) sender
{
    wxUnusedVar(sender);
    if ( impl )
    {
        wxMenuItem* menuitem = impl->GetWXPeer();
        if ( menuitem->GetMenu()->HandleCommandProcess(menuitem) == false )
        {
        }
     }
}

- (void) setEnabled:(BOOL) flag
{
    [super setEnabled:flag];
}

- (BOOL)validateMenuItem:(NSMenuItem *) menuItem
{
    wxUnusedVar(menuItem);
    if( impl )
    {
        wxMenuItem* wxmenuitem = impl->GetWXPeer();
        if ( wxmenuitem )
        {
            wxmenuitem->GetMenu()->HandleCommandUpdateStatus(wxmenuitem);
            return wxmenuitem->IsEnabled();
        }
    }
    return YES ;
}

- (void)setImplementation: (wxMenuItemImpl *) theImplementation
{
    impl = theImplementation;
}

- (wxMenuItemImpl*) implementation
{
    return impl;
}

@end

void wxMacCocoaMenuItemSetAccelerator( NSMenuItem* menuItem, wxAcceleratorEntry* entry )
{
    if ( entry == NULL )
    {
        [menuItem setKeyEquivalent:@""];
        return;
    }
         
    unsigned int modifiers = 0 ;
    int key = entry->GetKeyCode() ;
    if ( key )
    {
        if (entry->GetFlags() & wxACCEL_CTRL)
            modifiers |= NSCommandKeyMask;

        if (entry->GetFlags() & wxACCEL_RAW_CTRL)
            modifiers |= NSControlKeyMask;
        
        if (entry->GetFlags() & wxACCEL_ALT)
            modifiers |= NSAlternateKeyMask ;

        // this may be ignored later for alpha chars

        if (entry->GetFlags() & wxACCEL_SHIFT)
            modifiers |= NSShiftKeyMask ;

        unichar shortcut = 0;
        if ( key >= WXK_F1 && key <= WXK_F15 )
        {
            modifiers |= NSFunctionKeyMask ;
            shortcut = NSF1FunctionKey + ( key - WXK_F1 );
        }
        else
        {
            switch ( key )
            {
                case WXK_CLEAR :
                    modifiers |= NSFunctionKeyMask;
                    shortcut = NSDeleteCharacter ;
                    break ;

                case WXK_PAGEUP :
                    modifiers |= NSFunctionKeyMask;
                    shortcut = NSPageUpFunctionKey ;
                    break ;

                case WXK_PAGEDOWN :
                    modifiers |= NSFunctionKeyMask;
                    shortcut = NSPageDownFunctionKey ;
                    break ;

                case WXK_LEFT :
                    modifiers |= NSNumericPadKeyMask | NSFunctionKeyMask;
                    shortcut = NSLeftArrowFunctionKey ;
                    break ;

                case WXK_UP :
                    modifiers |= NSNumericPadKeyMask | NSFunctionKeyMask;
                    shortcut = NSUpArrowFunctionKey ;
                    break ;

                case WXK_RIGHT :
                    modifiers |= NSNumericPadKeyMask | NSFunctionKeyMask;
                    shortcut = NSRightArrowFunctionKey ;
                    break ;

                case WXK_DOWN :
                    modifiers |= NSNumericPadKeyMask | NSFunctionKeyMask;
                    shortcut = NSDownArrowFunctionKey ;
                    break ;

                case WXK_HOME :
                    modifiers |= NSFunctionKeyMask;
                    shortcut = NSHomeFunctionKey ;
                    break ;

                case WXK_END :
                    modifiers |= NSFunctionKeyMask;
                    shortcut = NSEndFunctionKey ;
                    break ;

                case WXK_NUMPAD_ENTER :
                    shortcut = NSEnterCharacter;
                    break;
                    
                case WXK_BACK :
                case WXK_RETURN :
                case WXK_TAB :
                case WXK_ESCAPE :
                default :
                    if(entry->GetFlags() & wxACCEL_SHIFT)
                        shortcut = toupper(key);
                    else
                        shortcut = tolower(key);
                    break ;
            }
        }

        [menuItem setKeyEquivalent:[NSString stringWithCharacters:&shortcut length:1]];
        [menuItem setKeyEquivalentModifierMask:modifiers];
    }
}

@interface NSMenuItem(PossibleMethods)
- (void)setHidden:(BOOL)hidden;
@end

class wxMenuItemCocoaImpl : public wxMenuItemImpl
{
public :
    wxMenuItemCocoaImpl( wxMenuItem* peer, NSMenuItem* item ) : wxMenuItemImpl(peer), m_osxMenuItem(item)
    {
        if ( ![m_osxMenuItem isSeparatorItem] )
            [(wxNSMenuItem*)m_osxMenuItem setImplementation:this];
    }

    ~wxMenuItemCocoaImpl();

    void SetBitmap( const wxBitmap& bitmap )
    {
        [m_osxMenuItem setImage:bitmap.GetNSImage()];
    }

    void Enable( bool enable )
    {
        [m_osxMenuItem setEnabled:enable];
    }

    void Check( bool check )
    {
        [m_osxMenuItem setState:( check ?  NSOnState :  NSOffState) ];
    }

    void Hide( bool hide )
    {
        // NB: setHidden is new as of 10.5 so we should not call it below there
        if ([m_osxMenuItem respondsToSelector:@selector(setHidden:)])
            [m_osxMenuItem setHidden:hide ];
        else
            wxLogDebug("wxMenuItemCocoaImpl::Hide not yet supported under OS X < 10.5");
    }

    void SetLabel( const wxString& text, wxAcceleratorEntry *entry )
    {
        wxCFStringRef cfText(text);
        [m_osxMenuItem setTitle:cfText.AsNSString()];

        wxMacCocoaMenuItemSetAccelerator( m_osxMenuItem, entry );
    }
    
    bool DoDefault();

    void * GetHMenuItem() { return m_osxMenuItem; }

protected :
    NSMenuItem* m_osxMenuItem ;
} ;

wxMenuItemCocoaImpl::~wxMenuItemCocoaImpl()
{
    if ( ![m_osxMenuItem isSeparatorItem] )
        [(wxNSMenuItem*)m_osxMenuItem setImplementation:nil];
    [m_osxMenuItem release];
}

bool wxMenuItemCocoaImpl::DoDefault()
{
    bool handled=false;
    int menuid = m_peer->GetId();
    
    NSApplication *theNSApplication = [NSApplication sharedApplication];
    if (menuid == wxID_OSX_HIDE)
    {
        [theNSApplication hide:nil];
        handled=true;
    }
    else if (menuid == wxID_OSX_HIDEOTHERS)
    {
        [theNSApplication hideOtherApplications:nil];
        handled=true;
    }
    else if (menuid == wxID_OSX_SHOWALL)
    {
        [theNSApplication unhideAllApplications:nil];
        handled=true;
    }
    else if (menuid == wxApp::s_macExitMenuItemId)
    {
        wxTheApp->ExitMainLoop();
    }
    return handled;
}

wxMenuItemImpl* wxMenuItemImpl::Create( wxMenuItem* peer, wxMenu *pParentMenu,
                       int menuid,
                       const wxString& text,
                       wxAcceleratorEntry *entry,
                       const wxString& WXUNUSED(strHelp),
                       wxItemKind kind,
                       wxMenu *pSubMenu )
{
    wxMenuItemImpl* c = NULL;
    NSMenuItem* item = nil;

    if ( kind == wxITEM_SEPARATOR )
    {
        item = [[NSMenuItem separatorItem] retain];
    }
    else
    {
        wxCFStringRef cfText(text);
        SEL selector = nil;
        bool targetSelf = false;
        if ( (pParentMenu == NULL || !pParentMenu->GetNoEventsMode()) && pSubMenu == NULL )
        {
            selector = wxOSXGetSelectorFromID(menuid);
            
            if ( selector == nil )
            {
                selector = @selector(clickedAction:);
                targetSelf = true;
            }
        }
        
        wxNSMenuItem* menuitem = [ [ wxNSMenuItem alloc ] initWithTitle:cfText.AsNSString() action:selector keyEquivalent:@""];
        if ( targetSelf )
            [menuitem setTarget:menuitem];
        
        if ( pSubMenu )
        {
            pSubMenu->GetPeer()->SetTitle( text );
            [menuitem setSubmenu:pSubMenu->GetHMenu()];
        }
        else
        {
            wxMacCocoaMenuItemSetAccelerator( menuitem, entry );
        }
        item = menuitem;
    }
    c = new wxMenuItemCocoaImpl( peer, item );
    return c;
}
