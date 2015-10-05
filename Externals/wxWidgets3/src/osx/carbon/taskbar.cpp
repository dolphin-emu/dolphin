///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/taskbar.cpp
// Purpose:     wxTaskBarIcon
// Author:      Ryan Norton
// Modified by:
// Created:     09/25/2004
// Copyright:   (c) 2004 Ryan Norton
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TASKBARICON

#include "wx/taskbar.h"

#ifndef WX_PRECOMP
    #include "wx/dcmemory.h"
    #include "wx/menu.h"
    #include "wx/toplevel.h"
    #include "wx/icon.h"
#endif

#include "wx/osx/private.h"

class wxTaskBarIconImpl
{
public:
    wxTaskBarIconImpl(wxTaskBarIcon* parent);
    virtual ~wxTaskBarIconImpl();

    virtual bool IsIconInstalled() const = 0;
    virtual bool SetIcon(const wxIcon& icon, const wxString& tooltip) = 0;
    virtual bool RemoveIcon() = 0;
    virtual bool PopupMenu(wxMenu *menu) = 0;

    wxMenu * CreatePopupMenu()
    { return m_parent->CreatePopupMenu(); }

    wxTaskBarIcon *m_parent;
    class wxTaskBarIconWindow *m_menuEventWindow;

    wxDECLARE_NO_COPY_CLASS(wxTaskBarIconImpl);
};

//-----------------------------------------------------------------------------
//
//  wxTaskBarIconWindow
//
//  Event handler for menus
//  NB: Since wxWindows in Mac HAVE to have parents we need this to be
//  a top level window...
//-----------------------------------------------------------------------------

class wxTaskBarIconWindow : public wxTopLevelWindow
{
public:
    wxTaskBarIconWindow(wxTaskBarIconImpl *impl)
        : wxTopLevelWindow(NULL, wxID_ANY, wxEmptyString), m_impl(impl)
    {
        Connect(
            -1, wxEVT_MENU,
            wxCommandEventHandler(wxTaskBarIconWindow::OnMenuEvent) );
    }

    void OnMenuEvent(wxCommandEvent& event)
    {
        m_impl->m_parent->ProcessEvent(event);
    }

private:
    wxTaskBarIconImpl *m_impl;
};

class wxDockTaskBarIcon : public wxTaskBarIconImpl
{
public:
    wxDockTaskBarIcon(wxTaskBarIcon* parent);
    virtual ~wxDockTaskBarIcon();

    virtual bool IsIconInstalled() const;
    virtual bool SetIcon(const wxIcon& icon, const wxString& tooltip);
    virtual bool RemoveIcon();
    virtual bool PopupMenu(wxMenu *menu);

    wxMenu* DoCreatePopupMenu();

    EventHandlerRef     m_eventHandlerRef;
    EventHandlerUPP     m_eventupp;
    wxWindow           *m_eventWindow;
    wxMenu             *m_pMenu;
    MenuRef             m_theLastMenu;
    bool                m_iconAdded;
};

// Forward declarations for utility functions for dock implementation
pascal OSStatus wxDockEventHandler(
    EventHandlerCallRef inHandlerCallRef,
    EventRef inEvent, void* pData );
wxMenu * wxDeepCopyMenu( wxMenu *menu );


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  wxTaskBarIconImpl
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

wxTaskBarIconImpl::wxTaskBarIconImpl(wxTaskBarIcon* parent)
    : m_parent(parent), m_menuEventWindow(new wxTaskBarIconWindow(this))
{
}

wxTaskBarIconImpl::~wxTaskBarIconImpl()
{
    delete m_menuEventWindow;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  wxDockTaskBarIcon
//
//  OS X Dock implementation of wxTaskBarIcon using Carbon
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//-----------------------------------------------------------------------------
// wxDockEventHandler
//
// This is the global Mac/Carbon event handler for the dock.
// We need this for two reasons:
// 1) To handle wxTaskBarIcon menu events (see below for why)
// 2) To handle events from the dock when it requests a menu
//-----------------------------------------------------------------------------
pascal OSStatus
wxDockEventHandler(EventHandlerCallRef WXUNUSED(inHandlerCallRef),
                   EventRef inEvent,
                   void *pData)
{
    // Get the parameters we want from the event
    wxDockTaskBarIcon* pTB = (wxDockTaskBarIcon*) pData;
    const UInt32 eventClass = GetEventClass(inEvent);
    const UInt32 eventKind = GetEventKind(inEvent);

    OSStatus err = eventNotHandledErr;

    // Handle wxTaskBar menu events (note that this is a global event handler
    // so it will actually get called by all commands/menus)
    if ((eventClass == kEventClassCommand) && (eventKind == kEventCommandProcess || eventKind == kEventCommandUpdateStatus ))
    {
        // if we have no taskbar menu quickly pass it back to wxApp
        if (pTB->m_pMenu != NULL)
        {
            // This is the real reason why we need this. Normally menus
            // get handled in wxMacAppEventHandler
            // However, in the case of a taskbar menu call
            // command.menu.menuRef IS NULL!
            // Which causes the wxApp handler just to skip it.

            // get the HICommand from the event
            HICommand command;
            if (GetEventParameter(inEvent, kEventParamDirectObject,
                typeHICommand, NULL,sizeof(HICommand), NULL, &command ) == noErr)
            {
                // Obtain the REAL menuRef and the menuItemIndex in the real menuRef
                //
                // NOTE: menuRef is generally used here for submenus, as
                // GetMenuItemRefCon could give an incorrect wxMenuItem if we pass
                // just the top level wxTaskBar menu
                MenuItemIndex menuItemIndex;
                MenuRef menuRef;
                MenuRef taskbarMenuRef = MAC_WXHMENU(pTB->m_pMenu->GetHMenu());

                // the next command is only successful if it was a command from the taskbar menu
                // otherwise we pass it on
                if (GetIndMenuItemWithCommandID(taskbarMenuRef,command.commandID,
                    1, &menuRef, &menuItemIndex ) == noErr)
                {
                    wxMenu* itemMenu = wxFindMenuFromMacMenu( menuRef ) ;
                    int id = wxMacCommandToId( command.commandID ) ;
                    wxMenuItem *item = NULL;

                    if (id != 0) // get the wxMenuItem reference from the MenuRef
                        GetMenuItemRefCon( menuRef, menuItemIndex, (URefCon*) &item );

                    if (item && itemMenu )
                    {
                        if ( eventKind == kEventCommandProcess )
                        {
                            if ( itemMenu->HandleCommandProcess( item ) )
                                err = noErr;
                        }
                        else if ( eventKind == kEventCommandUpdateStatus )
                        {
                            if ( itemMenu->HandleCommandUpdateStatus( item ) )
                                err = noErr;
                        }
                    }
                }
            }
        } //end if noErr on getting HICommand from event
    }
    else if ((eventClass == kEventClassApplication) && (eventKind == kEventAppGetDockTileMenu ))
    {
        // process the right click events
        // NB: This may result in double or even triple-creation of the menus
        // We need to do this for 2.4 compat, however
        wxTaskBarIconEvent downevt(wxEVT_TASKBAR_RIGHT_DOWN, NULL);
        pTB->m_parent->ProcessEvent(downevt);

        wxTaskBarIconEvent upevt(wxEVT_TASKBAR_RIGHT_UP, NULL);
        pTB->m_parent->ProcessEvent(upevt);

        // create popup menu
        wxMenu* menu = pTB->DoCreatePopupMenu();

        if (menu != NULL)
        {
            // note to self - a MenuRef *is* a MenuHandle
            MenuRef hMenu = MAC_WXHMENU(menu->GetHMenu());

            // When SetEventParameter is called it will decrement
            // the reference count of the menu - we need to make
            // sure it stays around in the wxMenu class here
            CFRetain(hMenu);

            // set the actual dock menu
            err = SetEventParameter(
                inEvent, kEventParamMenuRef,
                typeMenuRef, sizeof(MenuRef), &hMenu );
            verify_noerr( err );
        }
    }

    return err;
}

//-----------------------------------------------------------------------------
// wxDeepCopyMenu
//
// Performs a top-to-bottom copy of the input menu and all of its
// submenus.
//
// This is mostly needed for 2.4 compatibility. However wxPython and others
// still use this way of setting the taskbarmenu.
//-----------------------------------------------------------------------------
wxMenu * wxDeepCopyMenu( wxMenu *menu )
{
    if (menu == NULL)
        return NULL;

    // NB:  Here we have to perform a deep copy of the menu,
    // copying each and every menu item from menu to m_pMenu.
    // Other implementations use wxWindow::PopupMenu here,
    // which idle execution until the user selects something,
    // but since the Mac handles this internally, we can't -
    // and have no way at all to idle it while the dock menu
    // is being shown before menu goes out of scope (it may
    // not be on the heap, and may expire right after this function
    // is done - we need it to last until the carbon event is triggered -
    // that's when the user right clicks).
    //
    // Also, since there is no equal (assignment) operator
    // on either wxMenu or wxMenuItem, we have to do all the
    // dirty work ourselves.

    // perform a deep copy of the menu
    wxMenuItemList& theList = menu->GetMenuItems();
    wxMenuItemList::compatibility_iterator theNode = theList.GetFirst();

    // create the main menu
    wxMenu *m_pMenu = new wxMenu(menu->GetTitle());

    while (theNode != NULL)
    {
        wxMenuItem* theItem = theNode->GetData();
        m_pMenu->Append(
            new wxMenuItem(
                m_pMenu, // parent menu
                theItem->GetId(), // id
                theItem->GetItemLabel(), // text label
                theItem->GetHelp(), // status bar help string
                theItem->GetKind(), // menu flags - checkable, separator, etc.
                wxDeepCopyMenu(theItem->GetSubMenu()) )); // submenu

        theNode = theNode->GetNext();
    }

    return m_pMenu;
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon ctor
//
// Initializes the dock implementation of wxTaskBarIcon.
//
// Here we create some Mac-specific event handlers and UPPs.
//-----------------------------------------------------------------------------
wxDockTaskBarIcon::wxDockTaskBarIcon(wxTaskBarIcon* parent)
    :   wxTaskBarIconImpl(parent),
        m_eventHandlerRef(NULL), m_pMenu(NULL),
        m_theLastMenu(GetApplicationDockTileMenu()), m_iconAdded(false)
{
    // register the events that will return the dock menu
    EventTypeSpec tbEventList[] =
    {
        { kEventClassCommand, kEventProcessCommand },
        { kEventClassCommand, kEventCommandUpdateStatus },
        { kEventClassApplication, kEventAppGetDockTileMenu }
    };

    m_eventupp = NewEventHandlerUPP(wxDockEventHandler);
    wxASSERT(m_eventupp != NULL);

    OSStatus err = InstallApplicationEventHandler(
            m_eventupp,
            GetEventTypeCount(tbEventList), tbEventList,
            this, &m_eventHandlerRef);
    verify_noerr( err );
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon Destructor
//
// Cleans up mac events and restores the old icon to the dock
//-----------------------------------------------------------------------------
wxDockTaskBarIcon::~wxDockTaskBarIcon()
{
    // clean up event handler and event UPP
    RemoveEventHandler(m_eventHandlerRef);
    DisposeEventHandlerUPP(m_eventupp);

    // restore old icon and menu to the dock
    RemoveIcon();
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon::DoCreatePopupMenu
//
// Helper function that handles a request from the dock event handler
// to get the menu for the dock
//-----------------------------------------------------------------------------
wxMenu * wxDockTaskBarIcon::DoCreatePopupMenu()
{
    // get the menu from the parent
    wxMenu* theNewMenu = CreatePopupMenu();

    if (theNewMenu)
    {
        delete m_pMenu;
        m_pMenu = theNewMenu;
        m_pMenu->SetInvokingWindow(m_menuEventWindow);
    }

    // the return here can be one of three things
    // (in order of priority):
    // 1) User passed a menu from CreatePopupMenu override
    // 2) menu sent to and copied from PopupMenu
    // 3) If neither (1) or (2), then NULL
    //
    return m_pMenu;
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon::IsIconInstalled
//
// Returns whether or not the dock is not using the default image
//-----------------------------------------------------------------------------
bool wxDockTaskBarIcon::IsIconInstalled() const
{
    return m_iconAdded;
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon::SetIcon
//
// Sets the icon for the dock CGImage functions and SetApplicationDockTileImage
//-----------------------------------------------------------------------------
bool wxDockTaskBarIcon::SetIcon(const wxIcon& icon, const wxString& WXUNUSED(tooltip))
{
    // convert the wxIcon into a wxBitmap so we can perform some
    // wxBitmap operations with it
    wxBitmap bmp( icon );
    wxASSERT( bmp.IsOk() );

    // get the CGImageRef for the wxBitmap:
    // OSX builds only, but then the dock only exists in OSX
    CGImageRef pImage = (CGImageRef) bmp.CreateCGImage();
    wxASSERT( pImage != NULL );

    // actually set the dock image
    OSStatus err = SetApplicationDockTileImage( pImage );
    verify_noerr( err );

    // free the CGImage, now that it's referenced by the dock
    if (pImage != NULL)
        CGImageRelease( pImage );

    bool success = (err == noErr);
    m_iconAdded = success;

    return success;
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon::RemoveIcon
//
// Restores the old image for the dock via RestoreApplicationDockTileImage
//-----------------------------------------------------------------------------
bool wxDockTaskBarIcon::RemoveIcon()
{
    wxDELETE(m_pMenu);

    // restore old icon to the dock
    OSStatus err = RestoreApplicationDockTileImage();
    verify_noerr( err );

    // restore the old menu to the dock
    SetApplicationDockTileMenu( m_theLastMenu );

    bool success = (err == noErr);
    m_iconAdded = !success;

    return success;
}

//-----------------------------------------------------------------------------
// wxDockTaskBarIcon::PopupMenu
//
// 2.4 and wxPython method that "pops of the menu in the taskbar".
//
// In reality because of the way the dock menu works in carbon
// we just save the menu, and if the user didn't override CreatePopupMenu
// return the menu passed here, thus sort of getting the same effect.
//-----------------------------------------------------------------------------
bool wxDockTaskBarIcon::PopupMenu(wxMenu *menu)
{
    wxASSERT(menu != NULL);

    delete m_pMenu;

    // start copy of menu
    m_pMenu = wxDeepCopyMenu(menu);

    // finish up
    m_pMenu->SetInvokingWindow(m_menuEventWindow);

    return true;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  wxTaskBarIcon
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

wxIMPLEMENT_DYNAMIC_CLASS(wxTaskBarIcon, wxEvtHandler);

//-----------------------------------------------------------------------------
// wxTaskBarIcon Constructor
//
// Creates the backend
//
// Note that we only support DOCK currently as others require cocoa and
// also some require hacks and other such things. (MenuExtras are
// actually separate programs that also require a special undocumented id
// hack and other such fun stuff).
//-----------------------------------------------------------------------------
wxTaskBarIcon::wxTaskBarIcon(wxTaskBarIconType WXUNUSED_UNLESS_DEBUG(nType))
{
    wxASSERT_MSG(
        nType == wxTBI_DOCK,
        wxT("Only the wxTBI_DOCK implementation of wxTaskBarIcon on Mac-Carbon is currently supported!") );

    m_impl = new wxDockTaskBarIcon(this);
}

//-----------------------------------------------------------------------------
// wxTaskBarIcon Destructor
//
// Destroys the backend
//-----------------------------------------------------------------------------
wxTaskBarIcon::~wxTaskBarIcon()
{
    delete m_impl;
}

//-----------------------------------------------------------------------------
// wxTaskBarIcon::SetIcon
// wxTaskBarIcon::RemoveIcon
// wxTaskBarIcon::PopupMenu
//
// Just calls the backend version of the said function.
//-----------------------------------------------------------------------------
bool wxTaskBarIcon::IsIconInstalled() const
{ return m_impl->IsIconInstalled(); }

bool wxTaskBarIcon::SetIcon(const wxIcon& icon, const wxString& tooltip)
{ return m_impl->SetIcon(icon, tooltip); }

bool wxTaskBarIcon::RemoveIcon()
{ return m_impl->RemoveIcon(); }

bool wxTaskBarIcon::PopupMenu(wxMenu *menu)
{ return m_impl->PopupMenu(menu); }

#endif // wxUSE_TASKBARICON
