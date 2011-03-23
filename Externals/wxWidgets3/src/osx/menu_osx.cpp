/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/menu_osx.cpp
// Purpose:     wxMenu, wxMenuBar, wxMenuItem
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: menu_osx.cpp 67272 2011-03-22 06:44:08Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// headers & declarations
// ============================================================================

// wxWidgets headers
// -----------------

#include "wx/wxprec.h"

#if wxUSE_MENUS

#include "wx/menu.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/dialog.h"
    #include "wx/menuitem.h"
#endif

#include "wx/osx/private.h"

// other standard headers
// ----------------------
#include <string.h>

IMPLEMENT_ABSTRACT_CLASS( wxMenuImpl , wxObject )

wxMenuImpl::~wxMenuImpl()
{
}

// the (popup) menu title has this special menuid
static const int idMenuTitle = -3;

// ============================================================================
// implementation
// ============================================================================

// Menus

// Construct a menu with optional title (then use append)

void wxMenu::Init()
{
    m_doBreak = false;
    m_startRadioGroup = -1;
    m_allowRearrange = true;
    m_noEventsMode = false;

    m_peer = wxMenuImpl::Create( this, wxStripMenuCodes(m_title) );


    // if we have a title, insert it in the beginning of the menu
    if ( !m_title.empty() )
    {
        Append(idMenuTitle, m_title) ;
        AppendSeparator() ;
    }
}

wxMenu::~wxMenu()
{
    delete m_peer;
}

WXHMENU wxMenu::GetHMenu() const
{
    if ( m_peer )
        return m_peer->GetHMenu();
    return NULL;
}

void wxMenu::Break()
{
    // not available on the mac platform
}

void wxMenu::Attach(wxMenuBarBase *menubar)
{
    wxMenuBase::Attach(menubar);

    EndRadioGroup();
}

void wxMenu::SetAllowRearrange( bool allow )
{
    m_allowRearrange = allow;
}

void wxMenu::SetNoEventsMode( bool noEvents )
{
    m_noEventsMode = noEvents;
}

// function appends a new item or submenu to the menu
// append a new item or submenu to the menu
bool wxMenu::DoInsertOrAppend(wxMenuItem *pItem, size_t pos)
{
    wxASSERT_MSG( pItem != NULL, wxT("can't append NULL item to the menu") );
    GetPeer()->InsertOrAppend( pItem, pos );

    if ( pItem->IsSeparator() )
    {
        // nothing to do here
    }
    else
    {
        wxMenu *pSubMenu = pItem->GetSubMenu() ;
        if ( pSubMenu != NULL )
        {
            wxASSERT_MSG( pSubMenu->GetHMenu() != NULL , wxT("invalid submenu added"));
            pSubMenu->m_menuParent = this ;

            pSubMenu->DoRearrange();
        }
        else
        {
            if ( pItem->GetId() == idMenuTitle )
                pItem->GetMenu()->Enable( idMenuTitle, false );
        }
    }

    // if we're already attached to the menubar, we must update it
    if ( IsAttached() && GetMenuBar()->IsAttached() )
        GetMenuBar()->Refresh();

    return true ;
}

void wxMenu::EndRadioGroup()
{
    // we're not inside a radio group any longer
    m_startRadioGroup = -1;
}

wxMenuItem* wxMenu::DoAppend(wxMenuItem *item)
{
    wxCHECK_MSG( item, NULL, wxT("NULL item in wxMenu::DoAppend") );

    bool check = false;

    if ( item->GetKind() == wxITEM_RADIO )
    {
        int count = GetMenuItemCount();

        if ( m_startRadioGroup == -1 )
        {
            // start a new radio group
            m_startRadioGroup = count;

            // for now it has just one element
            item->SetAsRadioGroupStart();
            item->SetRadioGroupEnd(m_startRadioGroup);

            // ensure that we have a checked item in the radio group
            check = true;
        }
        else // extend the current radio group
        {
            // we need to update its end item
            item->SetRadioGroupStart(m_startRadioGroup);
            wxMenuItemList::compatibility_iterator node = GetMenuItems().Item(m_startRadioGroup);

            if ( node )
            {
                node->GetData()->SetRadioGroupEnd(count);
            }
            else
            {
                wxFAIL_MSG( wxT("where is the radio group start item?") );
            }
        }
    }
    else // not a radio item
    {
        EndRadioGroup();
    }

    if ( !wxMenuBase::DoAppend(item) || !DoInsertOrAppend(item) )
        return NULL;

    if ( check )
        // check the item initially
        item->Check(true);

    return item;
}

wxMenuItem* wxMenu::DoInsert(size_t pos, wxMenuItem *item)
{
    if (wxMenuBase::DoInsert(pos, item) && DoInsertOrAppend(item, pos))
        return item;

    return NULL;
}

wxMenuItem *wxMenu::DoRemove(wxMenuItem *item)
{
/*
    // we need to find the items position in the child list
    size_t pos;
    wxMenuItemList::compatibility_iterator node = GetMenuItems().GetFirst();

    for ( pos = 0; node; pos++ )
    {
        if ( node->GetData() == item )
            break;

        node = node->GetNext();
    }

    // DoRemove() (unlike Remove) can only be called for existing item!
    wxCHECK_MSG( node, NULL, wxT("bug in wxMenu::Remove logic") );

    wxOSXMenuRemoveItem(m_hMenu , pos );
    */
    GetPeer()->Remove( item );
    // and from internal data structures
    return wxMenuBase::DoRemove(item);
}

void wxMenu::SetTitle(const wxString& label)
{
    m_title = label ;
    GetPeer()->SetTitle( wxStripMenuCodes( label ) );
}

bool wxMenu::ProcessCommand(wxCommandEvent & event)
{
    bool processed = false;

    // Try the menu's event handler
    if ( /* !processed && */ GetEventHandler())
        processed = GetEventHandler()->SafelyProcessEvent(event);

    // Try the window the menu was popped up from
    // (and up through the hierarchy)
    wxWindow *win = GetWindow();
    if ( !processed && win )
        processed = win->HandleWindowEvent(event);

    return processed;
}

// ---------------------------------------------------------------------------
// other
// ---------------------------------------------------------------------------

// MacOS needs to know about submenus somewhere within this menu
// before it can be displayed, also hide special menu items
// like preferences that are handled by the OS
void wxMenu::DoRearrange()
{
    if ( !AllowRearrange() )
        return;

    wxMenuItem* previousItem = NULL ;
    size_t pos ;
    wxMenuItemList::compatibility_iterator node;
    wxMenuItem *item;

    for (pos = 0, node = GetMenuItems().GetFirst(); node; node = node->GetNext(), pos++)
    {
        item = (wxMenuItem *)node->GetData();
        wxMenu* subMenu = item->GetSubMenu() ;
        if (subMenu)
        {
            // already done
        }
        else // normal item
        {
            // what we do here is to hide the special items which are
            // shown in the application menu anyhow -- it doesn't make
            // sense to show them in their normal place as well
            if ( item->GetId() == wxApp::s_macAboutMenuItemId ||
                    item->GetId() == wxApp::s_macPreferencesMenuItemId ||
                    item->GetId() == wxApp::s_macExitMenuItemId )

            {
                item->GetPeer()->Hide( true );

                // also check for a separator which was used just to
                // separate this item from the others, so don't leave
                // separator at the menu start or end nor 2 consecutive
                // separators
                wxMenuItemList::compatibility_iterator nextNode = node->GetNext();
                wxMenuItem *next = nextNode ? nextNode->GetData() : NULL;

                wxMenuItem *sepToHide = 0;
                if ( !previousItem && next && next->IsSeparator() )
                {
                    // next (i.e. second as we must be first) item is
                    // the separator to hide
                    wxASSERT_MSG( pos == 0, wxT("should be the menu start") );
                    sepToHide = next;
                }
                else if ( GetMenuItems().GetCount() == pos + 1 &&
                            previousItem != NULL &&
                                previousItem->IsSeparator() )
                {
                    // prev item is a trailing separator we want to hide
                    sepToHide = previousItem;
                }
                else if ( previousItem && previousItem->IsSeparator() &&
                            next && next->IsSeparator() )
                {
                    // two consecutive separators, this is one too many
                    sepToHide = next;
                }

                if ( sepToHide )
                {
                    // hide the separator as well
                    sepToHide->GetPeer()->Hide( true );
                }
            }
        }

        previousItem = item ;
    }
}


bool wxMenu::HandleCommandUpdateStatus( wxMenuItem* item, wxWindow* senderWindow )
{
    int menuid = item ? item->GetId() : 0;
    wxUpdateUIEvent event(menuid);
    event.SetEventObject( this );

    bool processed = false;

    // Try the menu's event handler
    {
        wxEvtHandler *handler = GetEventHandler();
        if ( handler )
            processed = handler->ProcessEvent(event);
    }

    // Try the window the menu was popped up from
    // (and up through the hierarchy)
    if ( !processed )
    {
        wxWindow *win = GetWindow();
        if ( win )
            processed = win->HandleWindowEvent(event);
    }

    if ( !processed && senderWindow != NULL)
    {
        processed = senderWindow->HandleWindowEvent(event);
    }

    if ( processed )
    {
        // if anything changed, update the changed attribute
        if (event.GetSetText())
            SetLabel(menuid, event.GetText());
        if (event.GetSetChecked())
            Check(menuid, event.GetChecked());
        if (event.GetSetEnabled())
            Enable(menuid, event.GetEnabled());
    }
    else
    {
#if wxOSX_USE_CARBON
        // these two items are also managed by the Carbon Menu Manager, therefore we must
        // always reset them ourselves
        UInt32 cmd = 0;

        if ( menuid == wxApp::s_macExitMenuItemId )
        {
            cmd = kHICommandQuit;
        }
        else if (menuid == wxApp::s_macPreferencesMenuItemId )
        {
            cmd = kHICommandPreferences;
        }

        if ( cmd != 0 )
        {
            if ( !item->IsEnabled() || wxDialog::OSXHasModalDialogsOpen() )
                DisableMenuCommand( NULL , cmd ) ;
            else
                EnableMenuCommand( NULL , cmd ) ;

        }
#endif
    }

    return processed;
}

bool wxMenu::HandleCommandProcess( wxMenuItem* item, wxWindow* senderWindow )
{
    int menuid = item ? item->GetId() : 0;
    bool processed = false;
    if (item->IsCheckable())
        item->Check( !item->IsChecked() ) ;

    if ( SendEvent( menuid , item->IsCheckable() ? item->IsChecked() : -1 ) )
        processed = true ;
    else
    {
        if ( senderWindow != NULL )
        {
            wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED , menuid);
            event.SetEventObject(senderWindow);
            event.SetInt(item->IsCheckable() ? item->IsChecked() : -1);

            if ( senderWindow->HandleWindowEvent(event) )
                processed = true ;
        }
    }

    if(!processed && item)
    {
        processed = item->GetPeer()->DoDefault();  
    }
    
    return processed;
}

void wxMenu::HandleMenuItemHighlighted( wxMenuItem* item )
{
    int menuid = item ? item->GetId() : 0;
    wxMenuEvent wxevent(wxEVT_MENU_HIGHLIGHT, menuid, this);
    DoHandleMenuEvent( wxevent );
}

void wxMenu::HandleMenuOpened()
{
    wxMenuEvent wxevent(wxEVT_MENU_OPEN, 0, this);
    DoHandleMenuEvent( wxevent );
}

void wxMenu::HandleMenuClosed()
{
    wxMenuEvent wxevent(wxEVT_MENU_CLOSE, 0, this);
    DoHandleMenuEvent( wxevent );
}

bool wxMenu::DoHandleMenuEvent(wxEvent& wxevent)
{
    wxevent.SetEventObject(this);
    wxEvtHandler* handler = GetEventHandler();
    if (handler && handler->ProcessEvent(wxevent))
    {
        return true;
    }
    else
    {
        wxWindow *win = GetWindow();
        if (win)
        {
            if ( win->HandleWindowEvent(wxevent) )
                return true;
        }
    }
    return false;
}

// Menu Bar

/*

Mac Implementation note :

The Mac has only one global menubar, so we attempt to install the currently
active menubar from a frame, we currently don't take into account mdi-frames
which would ask for menu-merging

Secondly there is no mac api for changing a menubar that is not the current
menubar, so we have to wait for preparing the actual menubar until the
wxMenubar is to be used

We can in subsequent versions use MacInstallMenuBar to provide some sort of
auto-merge for MDI in case this will be necessary

*/

wxMenuBar* wxMenuBar::s_macInstalledMenuBar = NULL ;
wxMenuBar* wxMenuBar::s_macCommonMenuBar = NULL ;
bool     wxMenuBar::s_macAutoWindowMenu = true ;
WXHMENU  wxMenuBar::s_macWindowMenuHandle = NULL ;

const int firstMenuPos = 1; // to account for the 0th application menu on mac

void wxMenuBar::Init()
{
    m_eventHandler = this;
    m_menuBarFrame = NULL;
    m_rootMenu = new wxMenu();
    m_rootMenu->Attach(this);

    m_appleMenu = new wxMenu();
    m_appleMenu->SetAllowRearrange(false);

    // Create standard items unless the application explicitly disabled this by
    // setting the corresponding ids to wxID_NONE: although this is not
    // recommended, sometimes these items really don't make sense.
    if ( wxApp::s_macAboutMenuItemId != wxID_NONE )
    {
        wxString aboutLabel(_("About"));
        if ( wxTheApp )
            aboutLabel << ' ' << wxTheApp->GetAppDisplayName();
        else
            aboutLabel << "...";
        m_appleMenu->Append( wxApp::s_macAboutMenuItemId, aboutLabel);
        m_appleMenu->AppendSeparator();
    }

#if !wxOSX_USE_CARBON
    if ( wxApp::s_macPreferencesMenuItemId != wxID_NONE )
    {
        m_appleMenu->Append( wxApp::s_macPreferencesMenuItemId,
                             _("Preferences...") + "\tCtrl+," );
        m_appleMenu->AppendSeparator();
    }

    // standard menu items, handled in wxMenu::HandleCommandProcess(), see above:
    wxString hideLabel(_("Hide"));
    if ( wxTheApp )
        hideLabel << ' ' << wxTheApp->GetAppDisplayName();
    hideLabel << "\tCtrl+H";
    m_appleMenu->Append( wxID_OSX_HIDE, hideLabel );    
    m_appleMenu->Append( wxID_OSX_HIDEOTHERS, _("Hide Others")+"\tAlt+Ctrl+H" );    
    m_appleMenu->Append( wxID_OSX_SHOWALL, _("Show All") );    
    m_appleMenu->AppendSeparator();
    
    // Do always add "Quit" item unconditionally however, it can't be disabled.
    wxString quitLabel(_("Quit"));
    if ( wxTheApp )
        quitLabel << ' ' << wxTheApp->GetAppDisplayName();
    quitLabel << "\tCtrl+Q";
    m_appleMenu->Append( wxApp::s_macExitMenuItemId, quitLabel );
#endif // !wxOSX_USE_CARBON

    m_rootMenu->AppendSubMenu(m_appleMenu, "\x14") ;
}

wxMenuBar::wxMenuBar()
{
    Init();
}

wxMenuBar::wxMenuBar( long WXUNUSED(style) )
{
    Init();
}

wxMenuBar::wxMenuBar(size_t count, wxMenu *menus[], const wxString titles[], long WXUNUSED(style))
{
    Init();

    for ( size_t i = 0; i < count; i++ )
    {
        m_menus.Append(menus[i]);

        menus[i]->Attach(this);
        Append( menus[i], titles[i] );
    }
}

wxMenuBar::~wxMenuBar()
{
    if (s_macCommonMenuBar == this)
        s_macCommonMenuBar = NULL;

    if (s_macInstalledMenuBar == this)
    {
        s_macInstalledMenuBar = NULL;
    }
}

void wxMenuBar::Refresh(bool WXUNUSED(eraseBackground), const wxRect *WXUNUSED(rect))
{
    wxCHECK_RET( IsAttached(), wxT("can't refresh unatteched menubar") );
}

void wxMenuBar::MacInstallMenuBar()
{
    if ( s_macInstalledMenuBar == this )
        return ;

    m_rootMenu->GetPeer()->MakeRoot();
    
    // hide items in the apple menu that don't exist in the wx menubar
    
    int menuid = 0;
    wxMenuItem* appleItem = NULL;
    wxMenuItem* wxItem = NULL;

    menuid = wxApp::s_macAboutMenuItemId;
    appleItem = m_appleMenu->FindItem(menuid);
    wxItem = FindItem(menuid);
    if ( appleItem != NULL )
    {
        if ( wxItem == NULL )
            appleItem->GetPeer()->Hide();
        else 
            appleItem->SetItemLabel(wxItem->GetItemLabel());
    }
    
    menuid = wxApp::s_macPreferencesMenuItemId;
    appleItem = m_appleMenu->FindItem(menuid);
    wxItem = FindItem(menuid);
    if ( appleItem != NULL )
    {
        if ( wxItem == NULL )
            appleItem->GetPeer()->Hide();
        else 
            appleItem->SetItemLabel(wxItem->GetItemLabel());
    }
    
        
#if 0

    // if we have a mac help menu, clean it up before adding new items
    MenuHandle helpMenuHandle ;
    MenuItemIndex firstUserHelpMenuItem ;

    if ( UMAGetHelpMenuDontCreate( &helpMenuHandle , &firstUserHelpMenuItem) == noErr )
    {
        for ( int i = CountMenuItems( helpMenuHandle ) ; i >= firstUserHelpMenuItem ; --i )
            DeleteMenuItem( helpMenuHandle , i ) ;
    }
    else
    {
        helpMenuHandle = NULL ;
    }

    if ( wxApp::s_macPreferencesMenuItemId)
    {
        wxMenuItem *item = FindItem( wxApp::s_macPreferencesMenuItemId , NULL ) ;
        if ( item == NULL || !(item->IsEnabled()) )
            DisableMenuCommand( NULL , kHICommandPreferences ) ;
        else
            EnableMenuCommand( NULL , kHICommandPreferences ) ;
    }

    // Unlike preferences which may or may not exist, the Quit item should be always
    // enabled unless it is added by the application and then disabled, otherwise
    // a program would be required to add an item with wxID_EXIT in order to get the
    // Quit menu item to be enabled, which seems a bit burdensome.
    if ( wxApp::s_macExitMenuItemId)
    {
        wxMenuItem *item = FindItem( wxApp::s_macExitMenuItemId , NULL ) ;
        if ( item != NULL && !(item->IsEnabled()) )
            DisableMenuCommand( NULL , kHICommandQuit ) ;
        else
            EnableMenuCommand( NULL , kHICommandQuit ) ;
    }

    wxString strippedHelpMenuTitle = wxStripMenuCodes( wxApp::s_macHelpMenuTitleName ) ;
    wxString strippedTranslatedHelpMenuTitle = wxStripMenuCodes( wxString( _("&Help") ) ) ;
    wxMenuList::compatibility_iterator menuIter = m_menus.GetFirst();
    for (size_t i = 0; i < m_menus.GetCount(); i++, menuIter = menuIter->GetNext())
    {
        wxMenuItemList::compatibility_iterator node;
        wxMenuItem *item;
        wxMenu* menu = menuIter->GetData() , *subMenu = NULL ;
        wxString strippedMenuTitle = wxStripMenuCodes(m_titles[i]);

        if ( strippedMenuTitle == wxT("?") || strippedMenuTitle == strippedHelpMenuTitle || strippedMenuTitle == strippedTranslatedHelpMenuTitle )
        {
            for (node = menu->GetMenuItems().GetFirst(); node; node = node->GetNext())
            {
                item = (wxMenuItem *)node->GetData();
                subMenu = item->GetSubMenu() ;
                if (subMenu)
                {
                    UMAAppendMenuItem(mh, wxStripMenuCodes(item->GetText()) , wxFont::GetDefaultEncoding() );
                    MenuItemIndex position = CountMenuItems(mh);
                    ::SetMenuItemHierarchicalMenu(mh, position, MAC_WXHMENU(subMenu->GetHMenu()));
                }
                else
                {
                    if ( item->GetId() != wxApp::s_macAboutMenuItemId )
                    {
                        // we have found a user help menu and an item other than the about item,
                        // so we can create the mac help menu now, if we haven't created it yet
                        if ( helpMenuHandle == NULL )
                        {
                            if ( UMAGetHelpMenu( &helpMenuHandle , &firstUserHelpMenuItem) != noErr )
                            {
                                helpMenuHandle = NULL ;
                                break ;
                            }
                        }
                    }

                    if ( item->IsSeparator() )
                    {
                        if ( helpMenuHandle )
                            AppendMenuItemTextWithCFString( helpMenuHandle,
                                CFSTR(""), kMenuItemAttrSeparator, 0,NULL);
                    }
                    else
                    {
                        wxAcceleratorEntry*
                            entry = wxAcceleratorEntry::Create( item->GetItemLabel() ) ;

                        if ( item->GetId() == wxApp::s_macAboutMenuItemId )
                        {
                            // this will be taken care of below
                        }
                        else
                        {
                            if ( helpMenuHandle )
                            {
                                UMAAppendMenuItem(helpMenuHandle, wxStripMenuCodes(item->GetItemLabel()) , wxFont::GetDefaultEncoding(), entry);
                                SetMenuItemCommandID( helpMenuHandle , CountMenuItems(helpMenuHandle) , wxIdToMacCommand ( item->GetId() ) ) ;
                                SetMenuItemRefCon( helpMenuHandle , CountMenuItems(helpMenuHandle) , (URefCon) item ) ;
                            }
                        }

                        delete entry ;
                    }
                }
            }
        }

        else if ( ( m_titles[i] == wxT("Window") || m_titles[i] == wxT("&Window") )
                && GetAutoWindowMenu() )
        {
            if ( MacGetWindowMenuHMenu() == NULL )
            {
                CreateStandardWindowMenu( 0 , (MenuHandle*) &s_macWindowMenuHandle ) ;
            }

            MenuRef wm = (MenuRef)MacGetWindowMenuHMenu();
            if ( wm == NULL )
                break;

            // get the insertion point in the standard menu
            MenuItemIndex winListStart;
            GetIndMenuItemWithCommandID(wm,
                        kHICommandWindowListSeparator, 1, NULL, &winListStart);

            // add a separator so that the standard items and the custom items
            // aren't mixed together, but only if this is the first run
            OSStatus err = GetIndMenuItemWithCommandID(wm,
                        'WXWM', 1, NULL, NULL);

            if ( err == menuItemNotFoundErr )
            {
                InsertMenuItemTextWithCFString( wm,
                        CFSTR(""), winListStart-1, kMenuItemAttrSeparator, 'WXWM');
            }

            wxInsertMenuItemsInMenu(menu, wm, winListStart);
        }
        else
        {
            UMASetMenuTitle( MAC_WXHMENU(menu->GetHMenu()) , m_titles[i], GetFont().GetEncoding()  ) ;
            menu->MacBeforeDisplay(false) ;

            ::InsertMenu(MAC_WXHMENU(GetMenu(i)->GetHMenu()), 0);
        }
    }

    // take care of the about menu item wherever it is
    {
        wxMenu* aboutMenu ;
        wxMenuItem *aboutMenuItem = FindItem(wxApp::s_macAboutMenuItemId , &aboutMenu) ;
        if ( aboutMenuItem )
        {
            wxAcceleratorEntry*
                entry = wxAcceleratorEntry::Create( aboutMenuItem->GetItemLabel() ) ;
            UMASetMenuItemText( GetMenuHandle( kwxMacAppleMenuId ) , 1 , wxStripMenuCodes ( aboutMenuItem->GetItemLabel() ) , wxFont::GetDefaultEncoding() );
            UMAEnableMenuItem( GetMenuHandle( kwxMacAppleMenuId ) , 1 , true );
            SetMenuItemCommandID( GetMenuHandle( kwxMacAppleMenuId ) , 1 , kHICommandAbout ) ;
            SetMenuItemRefCon(GetMenuHandle( kwxMacAppleMenuId ) , 1 , (URefCon)aboutMenuItem ) ;
            UMASetMenuItemShortcut( GetMenuHandle( kwxMacAppleMenuId ) , 1 , entry ) ;
            delete entry;
        }
    }

    if ( GetAutoWindowMenu() )
    {
        if ( MacGetWindowMenuHMenu() == NULL )
            CreateStandardWindowMenu( 0 , (MenuHandle*) &s_macWindowMenuHandle ) ;

        InsertMenu( (MenuHandle) MacGetWindowMenuHMenu() , 0 ) ;
    }

    ::DrawMenuBar() ;
#endif

    s_macInstalledMenuBar = this;
}

void wxMenuBar::EnableTop(size_t pos, bool enable)
{
    wxCHECK_RET( IsAttached(), wxT("doesn't work with unattached menubars") );

    m_rootMenu->FindItemByPosition(pos+firstMenuPos)->Enable(enable);

    Refresh();
}

bool wxMenuBar::Enable(bool enable)
{
    wxCHECK_MSG( IsAttached(), false, wxT("doesn't work with unattached menubars") );

    size_t i;
    for (i = 0; i < GetMenuCount(); i++)
        EnableTop(i, enable);

    return true;
}

void wxMenuBar::SetMenuLabel(size_t pos, const wxString& label)
{
    wxCHECK_RET( pos < GetMenuCount(), wxT("invalid menu index") );

    GetMenu(pos)->SetTitle( label ) ;
}

wxString wxMenuBar::GetMenuLabel(size_t pos) const
{
    wxCHECK_MSG( pos < GetMenuCount(), wxEmptyString,
                 wxT("invalid menu index in wxMenuBar::GetMenuLabel") );

    return GetMenu(pos)->GetTitle();
}

// ---------------------------------------------------------------------------
// wxMenuBar construction
// ---------------------------------------------------------------------------

wxMenu *wxMenuBar::Replace(size_t pos, wxMenu *menu, const wxString& title)
{
    wxMenu *menuOld = wxMenuBarBase::Replace(pos, menu, title);
    if ( !menuOld )
        return NULL;

    wxMenuItem* item = m_rootMenu->FindItemByPosition(pos+firstMenuPos);
    m_rootMenu->Remove(item);
    m_rootMenu->Insert( pos+firstMenuPos, wxMenuItem::New( m_rootMenu, wxID_ANY, title, "", wxITEM_NORMAL, menu ) );

    return menuOld;
}

bool wxMenuBar::Insert(size_t pos, wxMenu *menu, const wxString& title)
{
    if ( !wxMenuBarBase::Insert(pos, menu, title) )
        return false;

    m_rootMenu->Insert( pos+firstMenuPos, wxMenuItem::New( m_rootMenu, wxID_ANY, title, "", wxITEM_NORMAL, menu ) );

    return true;
}

wxMenu *wxMenuBar::Remove(size_t pos)
{
    wxMenu *menu = wxMenuBarBase::Remove(pos);
    if ( !menu )
        return NULL;

    wxMenuItem* item = m_rootMenu->FindItemByPosition(pos+firstMenuPos);
    m_rootMenu->Remove(item);

    return menu;
}

bool wxMenuBar::Append(wxMenu *menu, const wxString& title)
{
    WXHMENU submenu = menu ? menu->GetHMenu() : 0;
        wxCHECK_MSG( submenu, false, wxT("can't append invalid menu to menubar") );

    if ( !wxMenuBarBase::Append(menu, title) )
        return false;

    m_rootMenu->AppendSubMenu(menu, title);
    menu->SetTitle(title);

    return true;
}

void wxMenuBar::Detach()
{
    wxMenuBarBase::Detach() ;
}

void wxMenuBar::Attach(wxFrame *frame)
{
    wxMenuBarBase::Attach( frame ) ;
}

#endif // wxUSE_MENUS
