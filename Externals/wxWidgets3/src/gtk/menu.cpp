/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/menu.cpp
// Purpose:     implementation of wxMenuBar and wxMenu classes for wxGTK
// Author:      Robert Roebling
// Id:          $Id: menu.cpp 66637 2011-01-07 21:36:17Z SC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_MENUS

#include "wx/menu.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/frame.h"
    #include "wx/bitmap.h"
    #include "wx/app.h"
#endif

#include "wx/accel.h"
#include "wx/stockitem.h"
#include "wx/gtk/private.h"
#include "wx/gtk/private/mnemonics.h"

// we use normal item but with a special id for the menu title
static const int wxGTK_TITLE_ID = -3;

// forward declare it as it's used by wxMenuBar too when using Hildon
extern "C"
{
    static void menuitem_activate(GtkWidget*, wxMenuItem* item);
}

#if wxUSE_ACCEL
static void wxGetGtkAccel(const wxMenuItem*, guint*, GdkModifierType*);
#endif

static void DoCommonMenuCallbackCode(wxMenu *menu, wxMenuEvent& event)
{
    event.SetEventObject( menu );

    wxEvtHandler* handler = menu->GetEventHandler();
    if (handler && handler->SafelyProcessEvent(event))
        return;

    wxWindow *win = menu->GetWindow();
    wxCHECK_RET( win, "event for a menu without associated window?" );

    win->HandleWindowEvent( event );
}

//-----------------------------------------------------------------------------
// wxMenuBar
//-----------------------------------------------------------------------------

void wxMenuBar::Init(size_t n, wxMenu *menus[], const wxString titles[], long style)
{
#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
    // Hildon window uses a single menu instead of a menu bar, so wxMenuBar is
    // the same as menu in this case
    m_widget =
    m_menubar = gtk_menu_new();
#else // !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    if (!PreCreation( NULL, wxDefaultPosition, wxDefaultSize ) ||
        !CreateBase( NULL, -1, wxDefaultPosition, wxDefaultSize, style, wxDefaultValidator, wxT("menubar") ))
    {
        wxFAIL_MSG( wxT("wxMenuBar creation failed") );
        return;
    }

    m_menubar = gtk_menu_bar_new();

    if (style & wxMB_DOCKABLE)
    {
        m_widget = gtk_handle_box_new();
        gtk_container_add(GTK_CONTAINER(m_widget), m_menubar);
        gtk_widget_show(m_menubar);
    }
    else
    {
        m_widget = m_menubar;
    }

    PostCreation();

    GTKApplyWidgetStyle();
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2/!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2

    g_object_ref(m_widget);

    for (size_t i = 0; i < n; ++i )
        Append(menus[i], titles[i]);
}

wxMenuBar::wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long style)
{
    Init(n, menus, titles, style);
}

wxMenuBar::wxMenuBar(long style)
{
    Init(0, NULL, NULL, style);
}

wxMenuBar::wxMenuBar()
{
    Init(0, NULL, NULL, 0);
}

// recursive helpers for wxMenuBar::Attach() and Detach(): they are called to
// associate the menus with the frame they belong to or dissociate them from it
namespace
{

void
DetachFromFrame(wxMenu* menu, wxFrame* frame)
{
    // support for native hot keys
    if (menu->m_accel)
    {
        // Note that wxGetTopLevelParent() is really needed because this frame
        // can be an MDI child frame which is a fake frame and not a TLW at all
        GtkWindow * const tlw = GTK_WINDOW(wxGetTopLevelParent(frame)->m_widget);
        if (g_slist_find(menu->m_accel->acceleratables, tlw))
            gtk_window_remove_accel_group(tlw, menu->m_accel);
    }

    wxMenuItemList::compatibility_iterator node = menu->GetMenuItems().GetFirst();
    while (node)
    {
        wxMenuItem *menuitem = node->GetData();
        if (menuitem->IsSubMenu())
            DetachFromFrame(menuitem->GetSubMenu(), frame);
        node = node->GetNext();
    }
}

void
AttachToFrame(wxMenu* menu, wxFrame* frame)
{
    // support for native hot keys
    if (menu->m_accel)
    {
        GtkWindow * const tlw = GTK_WINDOW(wxGetTopLevelParent(frame)->m_widget);
        if (!g_slist_find(menu->m_accel->acceleratables, tlw))
            gtk_window_add_accel_group(tlw, menu->m_accel);
    }

    wxMenuItemList::compatibility_iterator node = menu->GetMenuItems().GetFirst();
    while (node)
    {
        wxMenuItem *menuitem = node->GetData();
        if (menuitem->IsSubMenu())
            AttachToFrame(menuitem->GetSubMenu(), frame);
        node = node->GetNext();
    }
}

} // anonymous namespace

void wxMenuBar::SetLayoutDirection(wxLayoutDirection dir)
{
    if ( dir == wxLayout_Default )
    {
        const wxWindow *const frame = GetFrame();
        if ( frame )
        {
            // inherit layout from frame.
            dir = frame->GetLayoutDirection();
        }
        else // use global layout
        {
            dir = wxTheApp->GetLayoutDirection();
        }
    }

    if ( dir == wxLayout_Default )
        return;

    GTKSetLayout(m_menubar, dir);

    // also set the layout of all menus we already have (new ones will inherit
    // the current layout)
    for ( wxMenuList::compatibility_iterator node = m_menus.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxMenu *const menu = node->GetData();
        menu->SetLayoutDirection(dir);
    }
}

wxLayoutDirection wxMenuBar::GetLayoutDirection() const
{
    return GTKGetLayout(m_menubar);
}

void wxMenuBar::Attach(wxFrame *frame)
{
    wxMenuBarBase::Attach(frame);

    wxMenuList::compatibility_iterator node = m_menus.GetFirst();
    while (node)
    {
        wxMenu *menu = node->GetData();
        AttachToFrame( menu, frame );
        node = node->GetNext();
    }

    SetLayoutDirection(wxLayout_Default);
}

void wxMenuBar::Detach()
{
    wxMenuList::compatibility_iterator node = m_menus.GetFirst();
    while (node)
    {
        wxMenu *menu = node->GetData();
        DetachFromFrame( menu, m_menuBarFrame );
        node = node->GetNext();
    }

    wxMenuBarBase::Detach();
}

bool wxMenuBar::Append( wxMenu *menu, const wxString &title )
{
    if ( !wxMenuBarBase::Append( menu, title ) )
        return false;

    return GtkAppend(menu, title);
}

bool wxMenuBar::GtkAppend(wxMenu *menu, const wxString& title, int pos)
{
    menu->SetLayoutDirection(GetLayoutDirection());

#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
    // if the menu has only one item, append it directly to the top level menu
    // instead of inserting a useless submenu
    if ( menu->GetMenuItemCount() == 1 )
    {
        wxMenuItem * const item = menu->FindItemByPosition(0);

        // remove both mnemonics and accelerator: neither is useful under Maemo
        const wxString str(wxStripMenuCodes(item->GetItemLabel()));

        if ( item->IsSubMenu() )
            return GtkAppend(item->GetSubMenu(), str, pos);

        menu->m_owner = gtk_menu_item_new_with_mnemonic( wxGTK_CONV( str ) );

        g_signal_connect(menu->m_owner, "activate",
                         G_CALLBACK(menuitem_activate), item);
        item->SetMenuItem(menu->m_owner);
    }
    else
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2 /!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    {
        const wxString str(wxConvertMnemonicsToGTK(title));

        // This doesn't have much effect right now.
        menu->SetTitle( str );

        // The "m_owner" is the "menu item"
        menu->m_owner = gtk_menu_item_new_with_mnemonic( wxGTK_CONV( str ) );

        gtk_menu_item_set_submenu( GTK_MENU_ITEM(menu->m_owner), menu->m_menu );
    }

    gtk_widget_show( menu->m_owner );

    if (pos == -1)
        gtk_menu_shell_append( GTK_MENU_SHELL(m_menubar), menu->m_owner );
    else
        gtk_menu_shell_insert( GTK_MENU_SHELL(m_menubar), menu->m_owner, pos );

    if ( m_menuBarFrame )
        AttachToFrame( menu, m_menuBarFrame );

    return true;
}

bool wxMenuBar::Insert(size_t pos, wxMenu *menu, const wxString& title)
{
    if ( !wxMenuBarBase::Insert(pos, menu, title) )
        return false;

    // TODO

    if ( !GtkAppend(menu, title, (int)pos) )
        return false;

    return true;
}

wxMenu *wxMenuBar::Replace(size_t pos, wxMenu *menu, const wxString& title)
{
    // remove the old item and insert a new one
    wxMenu *menuOld = Remove(pos);
    if ( menuOld && !Insert(pos, menu, title) )
    {
        return NULL;
    }

    // either Insert() succeeded or Remove() failed and menuOld is NULL
    return menuOld;
}

wxMenu *wxMenuBar::Remove(size_t pos)
{
    wxMenu *menu = wxMenuBarBase::Remove(pos);
    if ( !menu )
        return NULL;

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu->m_owner), NULL);
    gtk_container_remove(GTK_CONTAINER(m_menubar), menu->m_owner);

    gtk_widget_destroy( menu->m_owner );
    menu->m_owner = NULL;

    DetachFromFrame( menu, m_menuBarFrame );

    return menu;
}

static int FindMenuItemRecursive( const wxMenu *menu, const wxString &menuString, const wxString &itemString )
{
    if (wxMenuItem::GetLabelText(menu->GetTitle()) == wxMenuItem::GetLabelText(menuString))
    {
        int res = menu->FindItem( itemString );
        if (res != wxNOT_FOUND)
            return res;
    }

    wxMenuItemList::compatibility_iterator node = menu->GetMenuItems().GetFirst();
    while (node)
    {
        wxMenuItem *item = node->GetData();
        if (item->IsSubMenu())
            return FindMenuItemRecursive(item->GetSubMenu(), menuString, itemString);

        node = node->GetNext();
    }

    return wxNOT_FOUND;
}

int wxMenuBar::FindMenuItem( const wxString &menuString, const wxString &itemString ) const
{
    wxMenuList::compatibility_iterator node = m_menus.GetFirst();
    while (node)
    {
        wxMenu *menu = node->GetData();
        int res = FindMenuItemRecursive( menu, menuString, itemString);
        if (res != -1)
            return res;
        node = node->GetNext();
    }

    return wxNOT_FOUND;
}

// Find a wxMenuItem using its id. Recurses down into sub-menus
static wxMenuItem* FindMenuItemByIdRecursive(const wxMenu* menu, int id)
{
    wxMenuItem* result = menu->FindChildItem(id);

    wxMenuItemList::compatibility_iterator node = menu->GetMenuItems().GetFirst();
    while ( node && result == NULL )
    {
        wxMenuItem *item = node->GetData();
        if (item->IsSubMenu())
        {
            result = FindMenuItemByIdRecursive( item->GetSubMenu(), id );
        }
        node = node->GetNext();
    }

    return result;
}

wxMenuItem* wxMenuBar::FindItem( int id, wxMenu **menuForItem ) const
{
    wxMenuItem* result = 0;
    wxMenuList::compatibility_iterator node = m_menus.GetFirst();
    while (node && result == 0)
    {
        wxMenu *menu = node->GetData();
        result = FindMenuItemByIdRecursive( menu, id );
        node = node->GetNext();
    }

    if ( menuForItem )
    {
        *menuForItem = result ? result->GetMenu() : NULL;
    }

    return result;
}

void wxMenuBar::EnableTop( size_t pos, bool flag )
{
    wxMenuList::compatibility_iterator node = m_menus.Item( pos );

    wxCHECK_RET( node, wxT("menu not found") );

    wxMenu* menu = node->GetData();

    if (menu->m_owner)
        gtk_widget_set_sensitive( menu->m_owner, flag );
}

wxString wxMenuBar::GetMenuLabel( size_t pos ) const
{
    wxMenuList::compatibility_iterator node = m_menus.Item( pos );

    wxCHECK_MSG( node, wxT("invalid"), wxT("menu not found") );

    wxMenu* menu = node->GetData();

    return menu->GetTitle();
}

void wxMenuBar::SetMenuLabel( size_t pos, const wxString& label )
{
    wxMenuList::compatibility_iterator node = m_menus.Item( pos );

    wxCHECK_RET( node, wxT("menu not found") );

    wxMenu* menu = node->GetData();

    const wxString str(wxConvertMnemonicsToGTK(label));

    menu->SetTitle( str );

    if (menu->m_owner)
        gtk_label_set_text_with_mnemonic( GTK_LABEL( GTK_BIN(menu->m_owner)->child), wxGTK_CONV(str) );
}

//-----------------------------------------------------------------------------
// "activate"
//-----------------------------------------------------------------------------

extern "C" {
static void menuitem_activate(GtkWidget*, wxMenuItem* item)
{
    if (!item->IsEnabled())
        return;

    int id = item->GetId();
    if (id == wxGTK_TITLE_ID)
    {
        // ignore events from the menu title
        return;
    }

    if (item->IsCheckable())
    {
        bool isReallyChecked = item->IsChecked(),
            isInternallyChecked = item->wxMenuItemBase::IsChecked();

        // ensure that the internal state is always consistent with what is
        // shown on the screen
        item->wxMenuItemBase::Check(isReallyChecked);

        // we must not report the events for the radio button going up nor the
        // events resulting from the calls to wxMenuItem::Check()
        if ( (item->GetKind() == wxITEM_RADIO && !isReallyChecked) ||
             (isInternallyChecked == isReallyChecked) )
        {
            return;
        }
    }

    wxMenu* menu = item->GetMenu();
    menu->SendEvent(id, item->IsCheckable() ? item->IsChecked() : -1);
}
}

//-----------------------------------------------------------------------------
// "select"
//-----------------------------------------------------------------------------

extern "C" {
static void menuitem_select(GtkWidget*, wxMenuItem* item)
{
    if (!item->IsEnabled())
        return;

    wxMenuEvent event(wxEVT_MENU_HIGHLIGHT, item->GetId());
    DoCommonMenuCallbackCode(item->GetMenu(), event);
}
}

//-----------------------------------------------------------------------------
// "deselect"
//-----------------------------------------------------------------------------

extern "C" {
static void menuitem_deselect(GtkWidget*, wxMenuItem* item)
{
    if (!item->IsEnabled())
        return;

    wxMenuEvent event( wxEVT_MENU_HIGHLIGHT, -1 );
    DoCommonMenuCallbackCode(item->GetMenu(), event);
}
}

//-----------------------------------------------------------------------------
// wxMenuItem
//-----------------------------------------------------------------------------

wxMenuItem *wxMenuItemBase::New(wxMenu *parentMenu,
                                int id,
                                const wxString& name,
                                const wxString& help,
                                wxItemKind kind,
                                wxMenu *subMenu)
{
    return new wxMenuItem(parentMenu, id, name, help, kind, subMenu);
}

wxMenuItem::wxMenuItem(wxMenu *parentMenu,
                       int id,
                       const wxString& text,
                       const wxString& help,
                       wxItemKind kind,
                       wxMenu *subMenu)
          : wxMenuItemBase(parentMenu, id, text, help, kind, subMenu)
{
    m_menuItem = NULL;
}

#if WXWIN_COMPATIBILITY_2_8
wxMenuItem::wxMenuItem(wxMenu *parentMenu,
                       int id,
                       const wxString& text,
                       const wxString& help,
                       bool isCheckable,
                       wxMenu *subMenu)
          : wxMenuItemBase(parentMenu, id, text, help,
                           isCheckable ? wxITEM_CHECK : wxITEM_NORMAL, subMenu)
{
    m_menuItem = NULL;
}
#endif

wxMenuItem::~wxMenuItem()
{
   // don't delete menu items, the menus take care of that
}

void wxMenuItem::SetItemLabel( const wxString& str )
{
#if wxUSE_ACCEL
    if (m_menuItem)
    {
        // remove old accelerator
        guint accel_key;
        GdkModifierType accel_mods;
        wxGetGtkAccel(this, &accel_key, &accel_mods);
        if (accel_key)
        {
            gtk_widget_remove_accelerator(
                m_menuItem, m_parentMenu->m_accel, accel_key, accel_mods);
        }
    }
#endif // wxUSE_ACCEL
    wxMenuItemBase::SetItemLabel(str);
    if (m_menuItem)
        SetGtkLabel();
}

void wxMenuItem::SetGtkLabel()
{
    const wxString text = wxConvertMnemonicsToGTK(m_text.BeforeFirst('\t'));
    GtkLabel* label = GTK_LABEL(GTK_BIN(m_menuItem)->child);
    gtk_label_set_text_with_mnemonic(label, wxGTK_CONV_SYS(text));
#if wxUSE_ACCEL
    guint accel_key;
    GdkModifierType accel_mods;
    wxGetGtkAccel(this, &accel_key, &accel_mods);
    if (accel_key)
    {
        gtk_widget_add_accelerator(
            m_menuItem, "activate", m_parentMenu->m_accel,
            accel_key, accel_mods, GTK_ACCEL_VISIBLE);
    }
#endif // wxUSE_ACCEL
}

void wxMenuItem::SetBitmap(const wxBitmap& bitmap)
{
    if (m_kind == wxITEM_NORMAL)
        m_bitmap = bitmap;
    else
        wxFAIL_MSG("only normal menu items can have bitmaps");
}

void wxMenuItem::Check( bool check )
{
    wxCHECK_RET( m_menuItem, wxT("invalid menu item") );

    if (check == m_isChecked)
        return;

    wxMenuItemBase::Check( check );

    switch ( GetKind() )
    {
        case wxITEM_CHECK:
        case wxITEM_RADIO:
            gtk_check_menu_item_set_active( (GtkCheckMenuItem*)m_menuItem, (gint)check );
            break;

        default:
            wxFAIL_MSG( wxT("can't check this item") );
    }
}

void wxMenuItem::Enable( bool enable )
{
    wxCHECK_RET( m_menuItem, wxT("invalid menu item") );

    gtk_widget_set_sensitive( m_menuItem, enable );
    wxMenuItemBase::Enable( enable );
}

bool wxMenuItem::IsChecked() const
{
    wxCHECK_MSG( m_menuItem, false, wxT("invalid menu item") );

    wxCHECK_MSG( IsCheckable(), false,
                 wxT("can't get state of uncheckable item!") );

    return ((GtkCheckMenuItem*)m_menuItem)->active != 0;
}

//-----------------------------------------------------------------------------
// wxMenu
//-----------------------------------------------------------------------------

extern "C" {
// "map" from m_menu
static void menu_map(GtkWidget*, wxMenu* menu)
{
    wxMenuEvent event(wxEVT_MENU_OPEN, menu->m_popupShown ? -1 : 0, menu);
    DoCommonMenuCallbackCode(menu, event);
}

// "hide" from m_menu
static void menu_hide(GtkWidget*, wxMenu* menu)
{
    wxMenuEvent event(wxEVT_MENU_CLOSE, menu->m_popupShown ? -1 : 0, menu);
    menu->m_popupShown = false;
    DoCommonMenuCallbackCode(menu, event);
}
}

// "can_activate_accel" from menu item
extern "C" {
static gboolean can_activate_accel(GtkWidget*, guint, wxMenu* menu)
{
    menu->UpdateUI();
    // always allow our "activate" handler to be called
    return true;
}
}

void wxMenu::Init()
{
    m_popupShown = false;

    m_accel = gtk_accel_group_new();
    m_menu = gtk_menu_new();
    // NB: keep reference to the menu so that it is not destroyed behind
    //     our back by GTK+ e.g. when it is removed from menubar:
    g_object_ref(m_menu);
    gtk_object_sink(GTK_OBJECT(m_menu));

    m_owner = NULL;

    // Tearoffs are entries, just like separators. So if we want this
    // menu to be a tear-off one, we just append a tearoff entry
    // immediately.
    if ( m_style & wxMENU_TEAROFF )
    {
        GtkWidget *tearoff = gtk_tearoff_menu_item_new();

        gtk_menu_shell_append(GTK_MENU_SHELL(m_menu), tearoff);
    }

    m_prevRadio = NULL;

    // append the title as the very first entry if we have it
    if ( !m_title.empty() )
    {
        Append(wxGTK_TITLE_ID, m_title);
        AppendSeparator();
    }

    // "show" occurs for sub-menus which are not showing, so use "map" instead
    g_signal_connect(m_menu, "map", G_CALLBACK(menu_map), this);
    g_signal_connect(m_menu, "hide", G_CALLBACK(menu_hide), this);
}

wxMenu::~wxMenu()
{
    // Destroying a menu generates a "hide" signal even if it's not shown
    // currently, so disconnect it to avoid dummy wxEVT_MENU_CLOSE events
    // generation.
    g_signal_handlers_disconnect_by_func(m_menu, (gpointer)menu_hide, this);

    // see wxMenu::Init
    g_object_unref(m_menu);

    // if the menu is inserted in another menu at this time, there was
    // one more reference to it:
    if (m_owner)
       gtk_widget_destroy(m_menu);

    g_object_unref(m_accel);
}

void wxMenu::SetLayoutDirection(const wxLayoutDirection dir)
{
    if ( m_owner )
        wxWindow::GTKSetLayout(m_owner, dir);
    //else: will be called later by wxMenuBar again
}

wxLayoutDirection wxMenu::GetLayoutDirection() const
{
    return wxWindow::GTKGetLayout(m_owner);
}

wxString wxMenu::GetTitle() const
{
    return wxConvertMnemonicsFromGTK(wxMenuBase::GetTitle());
}

bool wxMenu::GtkAppend(wxMenuItem *mitem, int pos)
{
    GtkWidget *menuItem;
    GtkWidget* prevRadio = m_prevRadio;
    m_prevRadio = NULL;
    switch (mitem->GetKind())
    {
        case wxITEM_SEPARATOR:
            menuItem = gtk_separator_menu_item_new();
            break;
        case wxITEM_CHECK:
            menuItem = gtk_check_menu_item_new_with_label("");
            break;
        case wxITEM_RADIO:
            {
                GSList* group = NULL;
                if (prevRadio)
                    group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(prevRadio));
                menuItem = gtk_radio_menu_item_new_with_label(group, "");
                m_prevRadio = menuItem;
            }
            break;
        default:
            wxFAIL_MSG("unexpected menu item kind");
            // fall through
        case wxITEM_NORMAL:
            const wxBitmap& bitmap = mitem->GetBitmap();
            const char* stockid;
            if (bitmap.IsOk())
            {
                // always use pixbuf, because pixmap mask does not
                // work with disabled images in some themes
                GtkWidget* image = gtk_image_new_from_pixbuf(bitmap.GetPixbuf());
                menuItem = gtk_image_menu_item_new_with_label("");
                gtk_widget_show(image);
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem), image);
            }
            else if ((stockid = wxGetStockGtkID(mitem->GetId())) != NULL)
                // use stock bitmap for this item if available on the assumption
                // that it never hurts to follow GTK+ conventions more closely
                menuItem = gtk_image_menu_item_new_from_stock(stockid, NULL);
            else
                menuItem = gtk_menu_item_new_with_label("");
            break;
    }
    mitem->SetMenuItem(menuItem);

    gtk_menu_shell_insert(GTK_MENU_SHELL(m_menu), menuItem, pos);

    gtk_widget_show( menuItem );

    if ( !mitem->IsSeparator() )
    {
        mitem->SetGtkLabel();
        g_signal_connect (menuItem, "select",
                          G_CALLBACK(menuitem_select), mitem);
        g_signal_connect (menuItem, "deselect",
                          G_CALLBACK(menuitem_deselect), mitem);

        if ( mitem->IsSubMenu() && mitem->GetKind() != wxITEM_RADIO && mitem->GetKind() != wxITEM_CHECK )
        {
            gtk_menu_item_set_submenu( GTK_MENU_ITEM(menuItem), mitem->GetSubMenu()->m_menu );

            gtk_widget_show( mitem->GetSubMenu()->m_menu );
        }
        else
        {
            g_signal_connect(menuItem, "can_activate_accel",
                G_CALLBACK(can_activate_accel), this);
            g_signal_connect (menuItem, "activate",
                              G_CALLBACK(menuitem_activate),
                              mitem);
        }
    }

    return true;
}

wxMenuItem* wxMenu::DoAppend(wxMenuItem *mitem)
{
    if (!GtkAppend(mitem))
        return NULL;

    return wxMenuBase::DoAppend(mitem);
}

wxMenuItem* wxMenu::DoInsert(size_t pos, wxMenuItem *item)
{
    if ( !wxMenuBase::DoInsert(pos, item) )
        return NULL;

    // TODO
    if ( !GtkAppend(item, (int)pos) )
        return NULL;

    return item;
}

wxMenuItem *wxMenu::DoRemove(wxMenuItem *item)
{
    if ( !wxMenuBase::DoRemove(item) )
        return NULL;

    GtkWidget * const mitem = item->GetMenuItem();
    if ( m_prevRadio == mitem )
    {
        // deleting an item starts a new radio group (has to as we shouldn't
        // keep a deleted pointer anyhow)
        m_prevRadio = NULL;
    }

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mitem), NULL);
    gtk_widget_destroy(mitem);
    item->SetMenuItem(NULL);

    return item;
}

void wxMenu::Attach(wxMenuBarBase *menubar)
{
    wxMenuBase::Attach(menubar);

    // inherit layout direction from menubar.
    SetLayoutDirection(menubar->GetLayoutDirection());
}

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

#if wxUSE_ACCEL

static wxString GetGtkHotKey( const wxMenuItem& item )
{
    wxString hotkey;

    wxAcceleratorEntry *accel = item.GetAccel();
    if ( accel )
    {
        int flags = accel->GetFlags();
        if ( flags & wxACCEL_ALT )
            hotkey += wxT("<alt>");
        if ( flags & wxACCEL_CTRL )
            hotkey += wxT("<control>");
        if ( flags & wxACCEL_SHIFT )
            hotkey += wxT("<shift>");

        int code = accel->GetKeyCode();
        switch ( code )
        {
            case WXK_F1:
            case WXK_F2:
            case WXK_F3:
            case WXK_F4:
            case WXK_F5:
            case WXK_F6:
            case WXK_F7:
            case WXK_F8:
            case WXK_F9:
            case WXK_F10:
            case WXK_F11:
            case WXK_F12:
            case WXK_F13:
            case WXK_F14:
            case WXK_F15:
            case WXK_F16:
            case WXK_F17:
            case WXK_F18:
            case WXK_F19:
            case WXK_F20:
            case WXK_F21:
            case WXK_F22:
            case WXK_F23:
            case WXK_F24:
                hotkey += wxString::Format(wxT("F%d"), code - WXK_F1 + 1);
                break;

                // TODO: we should use gdk_keyval_name() (a.k.a.
                //       XKeysymToString) here as well as hardcoding the keysym
                //       names this might be not portable
           case WXK_INSERT:
                hotkey << wxT("Insert" );
                break;
            case WXK_DELETE:
                hotkey << wxT("Delete" );
                break;
            case WXK_UP:
                hotkey << wxT("Up" );
                break;
            case WXK_DOWN:
                hotkey << wxT("Down" );
                break;
            case WXK_PAGEUP:
                hotkey << wxT("Page_Up" );
                break;
            case WXK_PAGEDOWN:
                hotkey << wxT("Page_Down" );
                break;
            case WXK_LEFT:
                hotkey << wxT("Left" );
                break;
            case WXK_RIGHT:
                hotkey << wxT("Right" );
                break;
            case WXK_HOME:
                hotkey << wxT("Home" );
                break;
            case WXK_END:
                hotkey << wxT("End" );
                break;
            case WXK_RETURN:
                hotkey << wxT("Return" );
                break;
            case WXK_BACK:
                hotkey << wxT("BackSpace" );
                break;
            case WXK_TAB:
                hotkey << wxT("Tab" );
                break;
            case WXK_ESCAPE:
                hotkey << wxT("Esc" );
                break;
            case WXK_SPACE:
                hotkey << wxT("space" );
                break;
            case WXK_MULTIPLY:
                hotkey << wxT("Multiply" );
                break;
            case WXK_ADD:
                hotkey << wxT("Add" );
                break;
            case WXK_SEPARATOR:
                hotkey << wxT("Separator" );
                break;
            case WXK_SUBTRACT:
                hotkey << wxT("Subtract" );
                break;
            case WXK_DECIMAL:
                hotkey << wxT("Decimal" );
                break;
            case WXK_DIVIDE:
                hotkey << wxT("Divide" );
                break;
            case WXK_CANCEL:
                hotkey << wxT("Cancel" );
                break;
            case WXK_CLEAR:
                hotkey << wxT("Clear" );
                break;
            case WXK_MENU:
                hotkey << wxT("Menu" );
                break;
            case WXK_PAUSE:
                hotkey << wxT("Pause" );
                break;
            case WXK_CAPITAL:
                hotkey << wxT("Capital" );
                break;
            case WXK_SELECT:
                hotkey << wxT("Select" );
                break;
            case WXK_PRINT:
                hotkey << wxT("Print" );
                break;
            case WXK_EXECUTE:
                hotkey << wxT("Execute" );
                break;
            case WXK_SNAPSHOT:
                hotkey << wxT("Snapshot" );
                break;
            case WXK_HELP:
                hotkey << wxT("Help" );
                break;
            case WXK_NUMLOCK:
                hotkey << wxT("Num_Lock" );
                break;
            case WXK_SCROLL:
                hotkey << wxT("Scroll_Lock" );
                break;
            case WXK_NUMPAD_INSERT:
                hotkey << wxT("KP_Insert" );
                break;
            case WXK_NUMPAD_DELETE:
                hotkey << wxT("KP_Delete" );
                break;
             case WXK_NUMPAD_SPACE:
                hotkey << wxT("KP_Space" );
                break;
            case WXK_NUMPAD_TAB:
                hotkey << wxT("KP_Tab" );
                break;
            case WXK_NUMPAD_ENTER:
                hotkey << wxT("KP_Enter" );
                break;
            case WXK_NUMPAD_F1: case WXK_NUMPAD_F2: case WXK_NUMPAD_F3:
            case WXK_NUMPAD_F4:
                hotkey += wxString::Format(wxT("KP_F%d"), code - WXK_NUMPAD_F1 + 1);
                break;
            case WXK_NUMPAD_HOME:
                hotkey << wxT("KP_Home" );
                break;
            case WXK_NUMPAD_LEFT:
                hotkey << wxT("KP_Left" );
                break;
             case WXK_NUMPAD_UP:
                hotkey << wxT("KP_Up" );
                break;
            case WXK_NUMPAD_RIGHT:
                hotkey << wxT("KP_Right" );
                break;
            case WXK_NUMPAD_DOWN:
                hotkey << wxT("KP_Down" );
                break;
            case WXK_NUMPAD_PAGEUP:
                hotkey << wxT("KP_Page_Up" );
                break;
            case WXK_NUMPAD_PAGEDOWN:
                hotkey << wxT("KP_Page_Down" );
                break;
            case WXK_NUMPAD_END:
                hotkey << wxT("KP_End" );
                break;
            case WXK_NUMPAD_BEGIN:
                hotkey << wxT("KP_Begin" );
                break;
            case WXK_NUMPAD_EQUAL:
                hotkey << wxT("KP_Equal" );
                break;
            case WXK_NUMPAD_MULTIPLY:
                hotkey << wxT("KP_Multiply" );
                break;
            case WXK_NUMPAD_ADD:
                hotkey << wxT("KP_Add" );
                break;
            case WXK_NUMPAD_SEPARATOR:
                hotkey << wxT("KP_Separator" );
                break;
            case WXK_NUMPAD_SUBTRACT:
                hotkey << wxT("KP_Subtract" );
                break;
            case WXK_NUMPAD_DECIMAL:
                hotkey << wxT("KP_Decimal" );
                break;
            case WXK_NUMPAD_DIVIDE:
                hotkey << wxT("KP_Divide" );
                break;
           case WXK_NUMPAD0: case WXK_NUMPAD1: case WXK_NUMPAD2:
           case WXK_NUMPAD3: case WXK_NUMPAD4: case WXK_NUMPAD5:
           case WXK_NUMPAD6: case WXK_NUMPAD7: case WXK_NUMPAD8: case WXK_NUMPAD9:
                hotkey += wxString::Format(wxT("KP_%d"), code - WXK_NUMPAD0);
                break;
            case WXK_WINDOWS_LEFT:
                hotkey << wxT("Super_L" );
                break;
            case WXK_WINDOWS_RIGHT:
                hotkey << wxT("Super_R" );
                break;
            case WXK_WINDOWS_MENU:
                hotkey << wxT("Menu" );
                break;
            case WXK_COMMAND:
                hotkey << wxT("Command" );
                break;
          /* These probably wouldn't work as there is no SpecialX in gdk/keynames.txt
            case WXK_SPECIAL1: case WXK_SPECIAL2: case WXK_SPECIAL3: case WXK_SPECIAL4:
            case WXK_SPECIAL5: case WXK_SPECIAL6: case WXK_SPECIAL7: case WXK_SPECIAL8:
            case WXK_SPECIAL9:  case WXK_SPECIAL10:  case WXK_SPECIAL11: case WXK_SPECIAL12:
            case WXK_SPECIAL13: case WXK_SPECIAL14: case WXK_SPECIAL15: case WXK_SPECIAL16:
            case WXK_SPECIAL17: case WXK_SPECIAL18: case WXK_SPECIAL19:  case WXK_SPECIAL20:
                hotkey += wxString::Format(wxT("Special%d"), code - WXK_SPECIAL1 + 1);
                break;
          */
                // if there are any other keys wxAcceleratorEntry::Create() may
                // return, we should process them here

            default:
                if ( code < 127 )
                {
                    const wxString
                        name = wxGTK_CONV_BACK_SYS(gdk_keyval_name((guint)code));
                    if ( !name.empty() )
                    {
                        hotkey << name;
                        break;
                    }
                }

                wxFAIL_MSG( wxT("unknown keyboard accel") );
        }

        delete accel;
    }

    return hotkey;
}

static void
wxGetGtkAccel(const wxMenuItem* item, guint* accel_key, GdkModifierType* accel_mods)
{
    *accel_key = 0;
    const wxString string = GetGtkHotKey(*item);
    if (!string.empty())
        gtk_accelerator_parse(wxGTK_CONV_SYS(string), accel_key, accel_mods);
    else
    {
        GtkStockItem stock_item;
        const char* stockid = wxGetStockGtkID(item->GetId());
        if (stockid && gtk_stock_lookup(stockid, &stock_item))
        {
            *accel_key = stock_item.keyval;
            *accel_mods = stock_item.modifier;
        }
    }
}
#endif // wxUSE_ACCEL

const char *wxGetStockGtkID(wxWindowID id)
{
    #define STOCKITEM(wx,gtk)      \
        case wx:                   \
            return gtk;

    #if GTK_CHECK_VERSION(2,6,0)
        #define STOCKITEM_26(wx,gtk) STOCKITEM(wx,gtk)
    #else
        #define STOCKITEM_26(wx,gtk)
    #endif

    #if GTK_CHECK_VERSION(2,8,0)
        #define STOCKITEM_28(wx,gtk) STOCKITEM(wx,gtk)
    #else
        #define STOCKITEM_28(wx,gtk)
    #endif

    #if GTK_CHECK_VERSION(2,10,0)
        #define STOCKITEM_210(wx,gtk) STOCKITEM(wx,gtk)
    #else
        #define STOCKITEM_210(wx,gtk)
    #endif


    switch (id)
    {
        STOCKITEM_26(wxID_ABOUT,         GTK_STOCK_ABOUT)
        STOCKITEM(wxID_ADD,              GTK_STOCK_ADD)
        STOCKITEM(wxID_APPLY,            GTK_STOCK_APPLY)
        STOCKITEM(wxID_BACKWARD,         GTK_STOCK_GO_BACK)
        STOCKITEM(wxID_BOLD,             GTK_STOCK_BOLD)
        STOCKITEM(wxID_BOTTOM,           GTK_STOCK_GOTO_BOTTOM)
        STOCKITEM(wxID_CANCEL,           GTK_STOCK_CANCEL)
        STOCKITEM(wxID_CDROM,            GTK_STOCK_CDROM)
        STOCKITEM(wxID_CLEAR,            GTK_STOCK_CLEAR)
        STOCKITEM(wxID_CLOSE,            GTK_STOCK_CLOSE)
        STOCKITEM(wxID_CONVERT,          GTK_STOCK_CONVERT)
        STOCKITEM(wxID_COPY,             GTK_STOCK_COPY)
        STOCKITEM(wxID_CUT,              GTK_STOCK_CUT)
        STOCKITEM(wxID_DELETE,           GTK_STOCK_DELETE)
        STOCKITEM(wxID_DOWN,             GTK_STOCK_GO_DOWN)
        STOCKITEM_26(wxID_EDIT,          GTK_STOCK_EDIT)
        STOCKITEM(wxID_EXECUTE,          GTK_STOCK_EXECUTE)
        STOCKITEM(wxID_EXIT,             GTK_STOCK_QUIT)
        STOCKITEM_26(wxID_FILE,          GTK_STOCK_FILE)
        STOCKITEM(wxID_FIND,             GTK_STOCK_FIND)
        STOCKITEM(wxID_FIRST,            GTK_STOCK_GOTO_FIRST)
        STOCKITEM(wxID_FLOPPY,           GTK_STOCK_FLOPPY)
        STOCKITEM(wxID_FORWARD,          GTK_STOCK_GO_FORWARD)
        STOCKITEM(wxID_HARDDISK,         GTK_STOCK_HARDDISK)
        STOCKITEM(wxID_HELP,             GTK_STOCK_HELP)
        STOCKITEM(wxID_HOME,             GTK_STOCK_HOME)
        STOCKITEM(wxID_INDENT,           GTK_STOCK_INDENT)
        STOCKITEM(wxID_INDEX,            GTK_STOCK_INDEX)
        STOCKITEM_28(wxID_INFO,           GTK_STOCK_INFO)
        STOCKITEM(wxID_ITALIC,           GTK_STOCK_ITALIC)
        STOCKITEM(wxID_JUMP_TO,          GTK_STOCK_JUMP_TO)
        STOCKITEM(wxID_JUSTIFY_CENTER,   GTK_STOCK_JUSTIFY_CENTER)
        STOCKITEM(wxID_JUSTIFY_FILL,     GTK_STOCK_JUSTIFY_FILL)
        STOCKITEM(wxID_JUSTIFY_LEFT,     GTK_STOCK_JUSTIFY_LEFT)
        STOCKITEM(wxID_JUSTIFY_RIGHT,    GTK_STOCK_JUSTIFY_RIGHT)
        STOCKITEM(wxID_LAST,             GTK_STOCK_GOTO_LAST)
        STOCKITEM(wxID_NETWORK,          GTK_STOCK_NETWORK)
        STOCKITEM(wxID_NEW,              GTK_STOCK_NEW)
        STOCKITEM(wxID_NO,               GTK_STOCK_NO)
        STOCKITEM(wxID_OK,               GTK_STOCK_OK)
        STOCKITEM(wxID_OPEN,             GTK_STOCK_OPEN)
        STOCKITEM(wxID_PASTE,            GTK_STOCK_PASTE)
        STOCKITEM(wxID_PREFERENCES,      GTK_STOCK_PREFERENCES)
        STOCKITEM(wxID_PREVIEW,          GTK_STOCK_PRINT_PREVIEW)
        STOCKITEM(wxID_PRINT,            GTK_STOCK_PRINT)
        STOCKITEM(wxID_PROPERTIES,       GTK_STOCK_PROPERTIES)
        STOCKITEM(wxID_REDO,             GTK_STOCK_REDO)
        STOCKITEM(wxID_REFRESH,          GTK_STOCK_REFRESH)
        STOCKITEM(wxID_REMOVE,           GTK_STOCK_REMOVE)
        STOCKITEM(wxID_REPLACE,          GTK_STOCK_FIND_AND_REPLACE)
        STOCKITEM(wxID_REVERT_TO_SAVED,  GTK_STOCK_REVERT_TO_SAVED)
        STOCKITEM(wxID_SAVE,             GTK_STOCK_SAVE)
        STOCKITEM(wxID_SAVEAS,           GTK_STOCK_SAVE_AS)
        STOCKITEM_210(wxID_SELECTALL,    GTK_STOCK_SELECT_ALL)
        STOCKITEM(wxID_SELECT_COLOR,     GTK_STOCK_SELECT_COLOR)
        STOCKITEM(wxID_SELECT_FONT,      GTK_STOCK_SELECT_FONT)
        STOCKITEM(wxID_SORT_ASCENDING,   GTK_STOCK_SORT_ASCENDING)
        STOCKITEM(wxID_SORT_DESCENDING,  GTK_STOCK_SORT_DESCENDING)
        STOCKITEM(wxID_SPELL_CHECK,      GTK_STOCK_SPELL_CHECK)
        STOCKITEM(wxID_STOP,             GTK_STOCK_STOP)
        STOCKITEM(wxID_STRIKETHROUGH,    GTK_STOCK_STRIKETHROUGH)
        STOCKITEM(wxID_TOP,              GTK_STOCK_GOTO_TOP)
        STOCKITEM(wxID_UNDELETE,         GTK_STOCK_UNDELETE)
        STOCKITEM(wxID_UNDERLINE,        GTK_STOCK_UNDERLINE)
        STOCKITEM(wxID_UNDO,             GTK_STOCK_UNDO)
        STOCKITEM(wxID_UNINDENT,         GTK_STOCK_UNINDENT)
        STOCKITEM(wxID_UP,               GTK_STOCK_GO_UP)
        STOCKITEM(wxID_YES,              GTK_STOCK_YES)
        STOCKITEM(wxID_ZOOM_100,         GTK_STOCK_ZOOM_100)
        STOCKITEM(wxID_ZOOM_FIT,         GTK_STOCK_ZOOM_FIT)
        STOCKITEM(wxID_ZOOM_IN,          GTK_STOCK_ZOOM_IN)
        STOCKITEM(wxID_ZOOM_OUT,         GTK_STOCK_ZOOM_OUT)

        default:
            break;
    };

    #undef STOCKITEM

    return NULL;
}

#endif // wxUSE_MENUS
