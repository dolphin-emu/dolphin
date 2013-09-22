/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/menu.cpp
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

#include "wx/menu.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/menuitem.h"
#endif

#include "wx/osx/private.h"
#include "wx/stockitem.h"

// other standard headers
// ----------------------
#include <string.h>

// under carbon there's no such thing as a MenuItemRef, everything is done
// on the 'parent' menu via index APIs (first line having index 1 !)
// so to make things still work, we store the wxMenuItemImpl instance as a
// RefCon at the respective menu line

class wxMenuItemCarbonImpl : public wxMenuItemImpl
{
public :
    wxMenuItemCarbonImpl( wxMenuItem* peer ) : wxMenuItemImpl(peer)
    {
        // the parent menu ref is only set, once the item has been attached
        m_parentMenuRef = NULL;
    }

    ~wxMenuItemCarbonImpl();

    void SetBitmap( const wxBitmap& bitmap )
    {
        MenuItemIndex i = FindMenuItemIndex() ;
        if ( i > 0 )
        {
            if ( bitmap.IsOk() )
            {
#if wxUSE_BMPBUTTON
                ControlButtonContentInfo info ;
                wxMacCreateBitmapButton( &info , bitmap ) ;
                if ( info.contentType != kControlNoContent )
                {
                    if ( info.contentType == kControlContentIconRef )
                        SetMenuItemIconHandle( m_parentMenuRef, i ,
                            kMenuIconRefType , (Handle) info.u.iconRef ) ;
                    else if ( info.contentType == kControlContentCGImageRef )
                       SetMenuItemIconHandle( m_parentMenuRef, i ,
                            kMenuCGImageRefType , (Handle) info.u.imageRef ) ;
                }
                wxMacReleaseBitmapButton( &info ) ;
#endif
            }
        }
    }

    void Enable( bool enable )
    {
        MenuItemIndex i = FindMenuItemIndex() ;
        if ( i > 0 )
        {

            if ( GetWXPeer()->GetId() == wxApp::s_macPreferencesMenuItemId)
            {
                if ( enable )
                    EnableMenuCommand( NULL , kHICommandPreferences ) ;
                else
                    DisableMenuCommand( NULL , kHICommandPreferences ) ;
            }
            else if ( GetWXPeer()->GetId() == wxApp::s_macExitMenuItemId)
            {
                if ( enable )
                    EnableMenuCommand( NULL , kHICommandQuit ) ;
                else
                    DisableMenuCommand( NULL , kHICommandQuit ) ;
            }

            if ( enable )
                EnableMenuItem(m_parentMenuRef , i);
            else
                DisableMenuItem(m_parentMenuRef , i);

            if ( GetWXPeer()->IsSubMenu() )
            {
                UMAEnableMenuItem( GetWXPeer()->GetSubMenu()->GetHMenu() , 0 , enable ) ;
            }
        }
    }

    void Check( bool check )
    {
        MenuItemIndex i = FindMenuItemIndex() ;
        if ( i > 0 )
        {
            if ( check )
                ::SetItemMark( m_parentMenuRef, i, 0x12 ) ; // checkmark
            else
                ::SetItemMark( m_parentMenuRef, i, 0 ) ; // no mark
        }
    }

    void Hide( bool hide )
    {
        MenuItemIndex i = FindMenuItemIndex() ;
        if ( i > 0 )
        {
            if ( hide )
                ChangeMenuItemAttributes( m_parentMenuRef, i, kMenuItemAttrHidden, 0 );
            else
                ChangeMenuItemAttributes( m_parentMenuRef, i, 0 , kMenuItemAttrHidden );
        }
    }

    void SetLabel( const wxString& text, wxAcceleratorEntry *entry )
    {
        MenuItemIndex i = FindMenuItemIndex() ;
        if ( i > 0 )
        {
            SetMenuItemTextWithCFString( m_parentMenuRef, i, wxCFStringRef(text));
            UMASetMenuItemShortcut( m_parentMenuRef, i , entry ) ;
         }
    }

    void * GetHMenuItem() { return NULL; }

    // Carbon Only

    void AttachToParent( MenuRef parentMenuRef, MenuItemIndex index )
    {
        m_parentMenuRef = parentMenuRef;
        if ( m_parentMenuRef && index > 0 )
            SetMenuItemRefCon( m_parentMenuRef, index, (URefCon) m_peer );
    }

    MenuItemIndex FindMenuItemIndex()
    {
        MenuItemIndex hit = 0 ;
        if ( m_parentMenuRef )
        {
            for ( MenuItemIndex i = 1 ; i <= CountMenuItems(m_parentMenuRef) ; ++i )
            {
                URefCon storedRef = 0;
                GetMenuItemRefCon(m_parentMenuRef, i, &storedRef );
                if ( storedRef == (URefCon) m_peer )
                {
                    hit = i;
                    break;
                }
            }
        }
        return hit;
    }
protected :
    MenuRef m_parentMenuRef;
} ;

//
// wxMenuImpl
//

class wxMenuCarbonImpl : public wxMenuImpl
{
public :
    wxMenuCarbonImpl( wxMenu* peer , MenuRef menu , MenuRef oldMenu , SInt16 menuId)
        : wxMenuImpl(peer), m_osxMenu(menu), m_oldMenuRef(oldMenu), m_menuId(menuId)
    {
    }

    virtual ~wxMenuCarbonImpl();

    virtual void InsertOrAppend(wxMenuItem *pItem, size_t pos)
    {
        // MacOS counts menu items from 1 and inserts after, therefore having the
        // same effect as wx 0 based and inserting before, we must correct pos
        // after however for updates to be correct

        MenuItemIndex index = pos;
        if ( pos == (size_t) -1 )
            index = CountMenuItems(m_osxMenu);

        if ( pItem->IsSeparator() )
        {
            InsertMenuItemTextWithCFString( m_osxMenu, CFSTR(""), index, kMenuItemAttrSeparator, 0);
            // now switch to the Carbon 1 based counting
            index += 1 ;
        }
        else
        {
            InsertMenuItemTextWithCFString( m_osxMenu, CFSTR("placeholder"), index, 0, 0 );

            // now switch to the Carbon 1 based counting
            index += 1 ;
            if ( pItem->IsSubMenu() )
            {
                MenuRef submenu = pItem->GetSubMenu()->GetHMenu();
                SetMenuItemHierarchicalMenu(m_osxMenu, index, submenu);
                // carbon is using the title of the submenu, eg in the menubar
                SetMenuTitleWithCFString(submenu, wxCFStringRef(pItem->GetItemLabelText()));
            }
            else
            {
                SetMenuItemCommandID( m_osxMenu, index , wxIdToMacCommand(pItem->GetId()) ) ;
            }
        }

        wxMenuItemCarbonImpl* impl = (wxMenuItemCarbonImpl*) pItem->GetPeer();
        impl->AttachToParent( m_osxMenu, index );
        // only now can all settings be updated correctly
        pItem->UpdateItemText();
        pItem->UpdateItemStatus();
        pItem->UpdateItemBitmap();
    }

    virtual void Remove( wxMenuItem *pItem )
    {
        wxMenuItemCarbonImpl* impl = (wxMenuItemCarbonImpl*) pItem->GetPeer();
        if ( impl )
        {
            MenuItemIndex i = impl->FindMenuItemIndex();
            if ( i > 0 )
            {
                DeleteMenuItem(m_osxMenu , i);
                impl->AttachToParent( NULL, 0 );
            }
        }
    }

    virtual void MakeRoot()
    {
        SetRootMenu( m_osxMenu );
    }

    virtual void SetTitle( const wxString& text )
    {
        SetMenuTitleWithCFString(m_osxMenu, wxCFStringRef(text));
    }

    WXHMENU GetHMenu() { return m_osxMenu; }

    virtual void PopUp( wxWindow *WXUNUSED(win), int x, int y )
    {
        long menuResult = ::PopUpMenuSelect(m_osxMenu, y, x, 0) ;
        if ( HiWord(menuResult) != 0 )
        {
            MenuCommand macid;
            GetMenuItemCommandID( GetMenuHandle(HiWord(menuResult)) , LoWord(menuResult) , &macid );
            int id = wxMacCommandToId( macid );
            wxMenuItem* item = NULL ;
            wxMenu* realmenu ;
            item = m_peer->FindItem( id, &realmenu ) ;
            if ( item )
            {
                m_peer->HandleCommandProcess(item, NULL );
            }
        }
    }

    static wxMenuImpl* Create( wxMenu* peer, const wxString& title );
    static wxMenuImpl* CreateRootMenu( wxMenu* peer );
protected :
    wxCFRef<MenuRef> m_osxMenu;
    MenuRef m_oldMenuRef;
    SInt16 m_menuId;
} ;

// static const short kwxMacAppleMenuId = 1 ;

// Find an item given the Macintosh Menu Reference

WX_DECLARE_HASH_MAP(WXHMENU, wxMenu*, wxPointerHash, wxPointerEqual, MacMenuMap);

static MacMenuMap wxWinMacMenuList;

wxMenu *wxFindMenuFromMacMenu(WXHMENU inMenuRef)
{
    MacMenuMap::iterator node = wxWinMacMenuList.find(inMenuRef);

    return (node == wxWinMacMenuList.end()) ? NULL : node->second;
}

void wxAssociateMenuWithMacMenu(WXHMENU inMenuRef, wxMenu *menu) ;
void wxAssociateMenuWithMacMenu(WXHMENU inMenuRef, wxMenu *menu)
{
    // adding NULL MenuRef is (first) surely a result of an error and
    // (secondly) breaks menu command processing
    wxCHECK_RET( inMenuRef != (WXHMENU) NULL, wxT("attempt to add a NULL MenuRef to menu list") );

    wxWinMacMenuList[inMenuRef] = menu;
}

void wxRemoveMacMenuAssociation(wxMenu *menu) ;
void wxRemoveMacMenuAssociation(wxMenu *menu)
{
   // iterate over all the elements in the class
    MacMenuMap::iterator it;
    for ( it = wxWinMacMenuList.begin(); it != wxWinMacMenuList.end(); ++it )
    {
        if ( it->second == menu )
        {
            wxWinMacMenuList.erase(it);
            break;
        }
    }
}

wxMenuCarbonImpl::~wxMenuCarbonImpl()
{
    wxRemoveMacMenuAssociation( GetWXPeer() );
    // restore previous menu
    m_osxMenu.reset();
    if ( m_menuId > 0 )
        MacDeleteMenu(m_menuId);
    if ( m_oldMenuRef )
        MacInsertMenu(m_oldMenuRef, -1);
}

wxMenuImpl* wxMenuImpl::Create( wxMenu* peer, const wxString& title )
{
    // create the menu
    static SInt16 s_macNextMenuId = 3;
    SInt16 menuId = s_macNextMenuId++;
    // save existing menu in case we're embedding into an application
    // or sharing outside UI elements.
    WXHMENU oldMenu = GetMenuHandle(menuId);
    if ( oldMenu )
        MacDeleteMenu(menuId);
    WXHMENU menu = NULL;
    CreateNewMenu( menuId , 0 , &menu ) ;
    if ( !menu )
    {
        wxLogLastError(wxT("CreateNewMenu failed"));
        if ( oldMenu )
            MacInsertMenu(oldMenu, -1);
        return NULL;
    }

    wxMenuImpl* c = new wxMenuCarbonImpl( peer, menu, oldMenu, menuId );
    c->SetTitle(title);
    wxAssociateMenuWithMacMenu( menu , peer ) ;
    return c;
}

//
//
//

wxMenuItemCarbonImpl::~wxMenuItemCarbonImpl()
{
}


wxMenuItemImpl* wxMenuItemImpl::Create( wxMenuItem* peer,
                        wxMenu * WXUNUSED(pParentMenu),
                       int WXUNUSED(id),
                       const wxString& WXUNUSED(text),
                       wxAcceleratorEntry *WXUNUSED(entry),
                       const wxString& WXUNUSED(strHelp),
                       wxItemKind WXUNUSED(kind),
                       wxMenu *WXUNUSED(pSubMenu) )
{
    wxMenuItemImpl* c = NULL;

    c = new wxMenuItemCarbonImpl( peer );
    return c;
}

void wxInsertMenuItemsInMenu(wxMenu* menu, MenuRef wm, MenuItemIndex insertAfter)
{
    wxMenuItemList::compatibility_iterator node;
    wxMenuItem *item;
    wxMenu *subMenu = NULL ;
    bool newItems = false;

    for (node = menu->GetMenuItems().GetFirst(); node; node = node->GetNext())
    {
        item = (wxMenuItem *)node->GetData();
        subMenu = item->GetSubMenu() ;
        if (subMenu)
        {
            wxInsertMenuItemsInMenu(subMenu, (MenuRef)subMenu->GetHMenu(), 0);
        }
        if ( item->IsSeparator() )
        {
            if ( wm && newItems)
                InsertMenuItemTextWithCFString( wm,
                    CFSTR(""), insertAfter, kMenuItemAttrSeparator, 0);

            newItems = false;
        }
        else
        {
            wxAcceleratorEntry*
                entry = wxAcceleratorEntry::Create( item->GetItemLabel() ) ;

            MenuItemIndex winListPos = (MenuItemIndex)-1;
            OSStatus err = GetIndMenuItemWithCommandID(wm,
                        wxIdToMacCommand ( item->GetId() ), 1, NULL, &winListPos);

            if ( wm && err == menuItemNotFoundErr )
            {
                // NB: the only way to determine whether or not we should add
                // a separator is to know if we've added menu items to the menu
                // before the separator.
                newItems = true;
                UMAInsertMenuItem(wm, wxStripMenuCodes(item->GetItemLabel()) , wxFont::GetDefaultEncoding(), insertAfter, entry);
                SetMenuItemCommandID( wm , insertAfter+1 , wxIdToMacCommand ( item->GetId() ) ) ;
                SetMenuItemRefCon( wm , insertAfter+1 , (URefCon) item ) ;
            }

            delete entry ;
        }
    }
}


