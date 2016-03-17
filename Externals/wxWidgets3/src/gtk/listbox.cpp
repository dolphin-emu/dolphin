/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/listbox.cpp
// Purpose:
// Author:      Robert Roebling
// Modified By: Ryan Norton (GtkTreeView implementation)
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_LISTBOX

#include "wx/listbox.h"

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/settings.h"
    #include "wx/checklst.h"
    #include "wx/arrstr.h"
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/eventsdisabler.h"
#include "wx/gtk/private/gtk2-compat.h"
#include "wx/gtk/private/object.h"
#include "wx/gtk/private/treeentry_gtk.h"
#include "wx/gtk/private/treeview.h"

#include <gdk/gdkkeysyms.h>
#ifdef __WXGTK3__
#include <gdk/gdkkeysyms-compat.h>
#endif

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool           g_blockEventsOnDrag;
extern bool           g_blockEventsOnScroll;



//-----------------------------------------------------------------------------
// Macro to tell which row the strings are in (1 if native checklist, 0 if not)
//-----------------------------------------------------------------------------

#if wxUSE_CHECKLISTBOX
#   define WXLISTBOX_DATACOLUMN_ARG(x)  (x->m_hasCheckBoxes ? 1 : 0)
#else
#   define WXLISTBOX_DATACOLUMN_ARG(x)  (0)
#endif // wxUSE_CHECKLISTBOX

#define WXLISTBOX_DATACOLUMN    WXLISTBOX_DATACOLUMN_ARG(this)

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

namespace
{

// Return the entry for the given listbox item.
wxTreeEntry *
GetEntry(GtkListStore *store, GtkTreeIter *iter, const wxListBox *listbox)
{
    wxTreeEntry* entry;
    gtk_tree_model_get(GTK_TREE_MODEL(store),
                       iter,
                       WXLISTBOX_DATACOLUMN_ARG(listbox),
                       &entry,
                       -1);
    g_object_unref(entry);
    return entry;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
// "row-activated"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_listbox_row_activated_callback(GtkTreeView        * WXUNUSED(treeview),
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  * WXUNUSED(col),
                                   wxListBox          *listbox)
{
    if (g_blockEventsOnDrag) return;
    if (g_blockEventsOnScroll) return;

    // This is triggered by either a double-click or a space press

    int sel = gtk_tree_path_get_indices(path)[0];

    listbox->GTKOnActivated(sel);
}
}

//-----------------------------------------------------------------------------
// "changed"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_listitem_changed_callback(GtkTreeSelection * WXUNUSED(selection),
                              wxListBox *listbox )
{
    if (g_blockEventsOnDrag) return;

    listbox->GTKOnSelectionChanged();
}

}

//-----------------------------------------------------------------------------
// "key_press_event"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_listbox_key_press_callback( GtkWidget *WXUNUSED(widget),
                                GdkEventKey *gdk_event,
                                wxListBox *listbox )
{
    if ((gdk_event->keyval == GDK_Return) ||
        (gdk_event->keyval == GDK_ISO_Enter) ||
        (gdk_event->keyval == GDK_KP_Enter))
    {
        int index = -1;
        if (!listbox->HasMultipleSelection())
            index = listbox->GetSelection();
        else
        {
            wxArrayInt sels;
            if (listbox->GetSelections( sels ) < 1)
                return FALSE;
            index = sels[0];
        }

        if (index != wxNOT_FOUND)
        {
            listbox->GTKOnActivated(index);

//          wxMac and wxMSW always invoke default action
//          if (!ret)
            {
                // DClick not handled -> invoke default action
                wxWindow *tlw = wxGetTopLevelParent( listbox );
                if (tlw)
                {
                    GtkWindow *gtk_window = GTK_WINDOW( tlw->GetHandle() );
                    if (gtk_window)
                        gtk_window_activate_default( gtk_window );
                }
            }

            // Always intercept, otherwise we'd get another dclick
            // event from row_activated
            return TRUE;
        }
    }

    return FALSE;
}
}

//-----------------------------------------------------------------------------
// GtkTreeEntry destruction (to destroy client data)
//-----------------------------------------------------------------------------

extern "C" {
static void tree_entry_destroy_cb(wxTreeEntry* entry,
                                      wxListBox* listbox)
{
    if (listbox->HasClientObjectData())
    {
        void* userdata = wx_tree_entry_get_userdata(entry);
        if (userdata)
            delete (wxClientData *)userdata;
    }
}
}

//-----------------------------------------------------------------------------
// Sorting callback (standard CmpNoCase return value)
//-----------------------------------------------------------------------------

extern "C" {
static int
sort_callback(GtkTreeModel*, GtkTreeIter* a, GtkTreeIter* b, void* data)
{
    wxListBox* listbox = static_cast<wxListBox*>(data);
    wxTreeEntry* entry1 = GetEntry(listbox->m_liststore, a, listbox);
    wxCHECK_MSG(entry1, 0, wxT("Could not get first entry"));

    wxTreeEntry* entry2 = GetEntry(listbox->m_liststore, b, listbox);
    wxCHECK_MSG(entry2, 0, wxT("Could not get second entry"));

    //We compare collate keys here instead of calling g_utf8_collate
    //as it is rather slow (and even the docs recommend this)
    return strcmp(wx_tree_entry_get_collate_key(entry1),
                  wx_tree_entry_get_collate_key(entry2)) >= 0;
}
}

//-----------------------------------------------------------------------------
// Searching callback (TRUE == not equal, FALSE == equal)
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
search_callback(GtkTreeModel*, int, const char* key, GtkTreeIter* iter, void* data)
{
    wxListBox* listbox = static_cast<wxListBox*>(data);
    wxTreeEntry* entry = GetEntry(listbox->m_liststore, iter, listbox);
    wxCHECK_MSG(entry, true, "could not get entry");

    wxGtkString keyc(g_utf8_collate_key(key, -1));

    return strncmp(keyc, wx_tree_entry_get_collate_key(entry), strlen(keyc));
}
}

//-----------------------------------------------------------------------------
// wxListBox
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

void wxListBox::Init()
{
    m_treeview = NULL;
#if wxUSE_CHECKLISTBOX
    m_hasCheckBoxes = false;
#endif // wxUSE_CHECKLISTBOX
}

bool wxListBox::Create( wxWindow *parent, wxWindowID id,
                        const wxPoint &pos, const wxSize &size,
                        const wxArrayString& choices,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    wxCArrayString chs(choices);

    return Create( parent, id, pos, size, chs.GetCount(), chs.GetStrings(),
                   style, validator, name );
}

bool wxListBox::Create( wxWindow *parent, wxWindowID id,
                        const wxPoint &pos, const wxSize &size,
                        int n, const wxString choices[],
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxListBox creation failed") );
        return false;
    }

    m_widget = gtk_scrolled_window_new( NULL, NULL );
    g_object_ref(m_widget);
    if (style & wxLB_ALWAYS_SB)
    {
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_widget),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    }
    else
    {
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_widget),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    }


    GTKScrolledWindowSetBorder(m_widget, style);

    m_treeview = GTK_TREE_VIEW( gtk_tree_view_new( ) );

    //wxListBox doesn't have a header :)
    //NB: If enabled SetFirstItem doesn't work correctly
    gtk_tree_view_set_headers_visible(m_treeview, FALSE);

#if wxUSE_CHECKLISTBOX
    if(m_hasCheckBoxes)
        ((wxCheckListBox*)this)->DoCreateCheckList();
#endif // wxUSE_CHECKLISTBOX

    // Create the data column
    gtk_tree_view_insert_column_with_attributes(m_treeview, -1, "",
                                                gtk_cell_renderer_text_new(),
                                                "text",
                                                WXLISTBOX_DATACOLUMN, NULL);

    // Now create+set the model (GtkListStore) - first argument # of columns
#if wxUSE_CHECKLISTBOX
    if(m_hasCheckBoxes)
        m_liststore = gtk_list_store_new(2, G_TYPE_BOOLEAN,
                                            WX_TYPE_TREE_ENTRY);
    else
#endif
        m_liststore = gtk_list_store_new(1, WX_TYPE_TREE_ENTRY);

    gtk_tree_view_set_model(m_treeview, GTK_TREE_MODEL(m_liststore));

    g_object_unref (m_liststore); //free on treeview destruction

    // Disable the pop-up textctrl that enables searching - note that
    // the docs specify that even if this disabled (which we are doing)
    // the user can still have it through the start-interactive-search
    // key binding...either way we want to provide a searchequal callback
    // NB: If this is enabled a doubleclick event (activate) gets sent
    //     on a successful search
    gtk_tree_view_set_search_column(m_treeview, WXLISTBOX_DATACOLUMN);
    gtk_tree_view_set_search_equal_func(m_treeview, search_callback, this, NULL);

    gtk_tree_view_set_enable_search(m_treeview, FALSE);

    GtkSelectionMode mode;
    // GTK_SELECTION_EXTENDED is a deprecated synonym for GTK_SELECTION_MULTIPLE
    if ( style & (wxLB_MULTIPLE | wxLB_EXTENDED) )
    {
        mode = GTK_SELECTION_MULTIPLE;
    }
    else // no multi-selection flags specified
    {
        m_windowStyle |= wxLB_SINGLE;

        // Notice that we must use BROWSE and not GTK_SELECTION_SINGLE because
        // the latter allows to not select any items at all while a single
        // selection listbox is supposed to always have a selection (at least
        // once the user selected something, it might not have any initially).
        mode = GTK_SELECTION_BROWSE;
    }

    GtkTreeSelection* selection = gtk_tree_view_get_selection( m_treeview );
    gtk_tree_selection_set_mode( selection, mode );

    // Handle sortable stuff
    if(HasFlag(wxLB_SORT))
    {
        // Setup sorting in ascending (wx) order
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(m_liststore),
                                             WXLISTBOX_DATACOLUMN,
                                             GTK_SORT_ASCENDING);

        // Set the sort callback
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(m_liststore),
                                        WXLISTBOX_DATACOLUMN,
                                        sort_callback,
                                        this, //userdata
                                        NULL //"destroy notifier"
                                       );
    }


    gtk_container_add (GTK_CONTAINER (m_widget), GTK_WIDGET(m_treeview) );

    gtk_widget_show( GTK_WIDGET(m_treeview) );
    m_focusWidget = GTK_WIDGET(m_treeview);

    Append(n, choices); // insert initial items

    // generate dclick events
    g_signal_connect_after(m_treeview, "row-activated",
                     G_CALLBACK(gtk_listbox_row_activated_callback), this);

    // for intercepting dclick generation by <ENTER>
    g_signal_connect (m_treeview, "key_press_event",
                      G_CALLBACK (gtk_listbox_key_press_callback),
                           this);
    m_parent->DoAddChild( this );

    PostCreation(size);
    SetInitialSize(size); // need this too because this is a wxControlWithItems

    g_signal_connect_after (selection, "changed",
                            G_CALLBACK (gtk_listitem_changed_callback), this);

    return true;
}

wxListBox::~wxListBox()
{
    if (m_treeview)
    {
        GTKDisconnect(m_treeview);
        GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);
        if (selection)
            GTKDisconnect(selection);
    }

    Clear();
}

void wxListBox::GTKDisableEvents()
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection( m_treeview );

    g_signal_handlers_block_by_func(selection,
                                (gpointer) gtk_listitem_changed_callback, this);
}

void wxListBox::GTKEnableEvents()
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection( m_treeview );

    g_signal_handlers_unblock_by_func(selection,
                                (gpointer) gtk_listitem_changed_callback, this);

    UpdateOldSelections();
}


void wxListBox::Update()
{
    wxWindow::Update();

    if (m_treeview)
        gdk_window_process_updates(gtk_widget_get_window(GTK_WIDGET(m_treeview)), true);
}

// ----------------------------------------------------------------------------
// adding items
// ----------------------------------------------------------------------------

int wxListBox::DoInsertItems(const wxArrayStringsAdapter& items,
                             unsigned int pos,
                             void **clientData,
                             wxClientDataType type)
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox") );

    InvalidateBestSize();
    int n = DoInsertItemsInLoop(items, pos, clientData, type);
    UpdateOldSelections();
    return n;
}

int wxListBox::DoInsertOneItem(const wxString& item, unsigned int pos)
{
    wxTreeEntry* entry = wx_tree_entry_new();
    wx_tree_entry_set_label(entry, wxGTK_CONV(item));
    wx_tree_entry_set_destroy_func(entry, (wxTreeEntryDestroy)tree_entry_destroy_cb, this);

#if wxUSE_CHECKLISTBOX
    int entryCol = int(m_hasCheckBoxes);
#else
    int entryCol = 0;
#endif
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(m_liststore, &iter, pos, entryCol, entry, -1);
    g_object_unref(entry);

    if (HasFlag(wxLB_SORT))
        pos = GTKGetIndexFor(iter);

    return pos;
}

// ----------------------------------------------------------------------------
// deleting items
// ----------------------------------------------------------------------------

void wxListBox::DoClear()
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    {
        wxGtkEventsDisabler<wxListBox> noEvents(this);

        InvalidateBestSize();

        gtk_list_store_clear( m_liststore ); /* well, THAT was easy :) */
    }

    UpdateOldSelections();
}

void wxListBox::DoDeleteOneItem(unsigned int n)
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    InvalidateBestSize();

    wxGtkEventsDisabler<wxListBox> noEvents(this);

    GtkTreeIter iter;
    wxCHECK_RET( GTKGetIteratorFor(n, &iter), wxT("wrong listbox index") );

    // this returns false if iter is invalid (e.g. deleting item at end) but
    // since we don't use iter, we ignore the return value
    gtk_list_store_remove(m_liststore, &iter);
}

// ----------------------------------------------------------------------------
// helper functions for working with iterators
// ----------------------------------------------------------------------------

bool wxListBox::GTKGetIteratorFor(unsigned pos, GtkTreeIter *iter) const
{
    if ( !gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(m_liststore),
                                        iter, NULL, pos) )
    {
        wxLogDebug(wxT("gtk_tree_model_iter_nth_child(%u) failed"), pos);
        return false;
    }

    return true;
}

int wxListBox::GTKGetIndexFor(GtkTreeIter& iter) const
{
    wxGtkTreePath path(
        gtk_tree_model_get_path(GTK_TREE_MODEL(m_liststore), &iter));

    gint* pIntPath = gtk_tree_path_get_indices(path);

    wxCHECK_MSG( pIntPath, wxNOT_FOUND, wxT("failed to get iterator path") );

    return pIntPath[0];
}

// get GtkTreeEntry from position (note: you need to g_unref it if valid)
wxTreeEntry* wxListBox::GTKGetEntry(unsigned n) const
{
    GtkTreeIter iter;
    if ( !GTKGetIteratorFor(n, &iter) )
        return NULL;

    return GetEntry(m_liststore, &iter, this);
}

// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------

void* wxListBox::DoGetItemClientData(unsigned int n) const
{
    wxTreeEntry* entry = GTKGetEntry(n);
    wxCHECK_MSG(entry, NULL, wxT("could not get entry"));

    return wx_tree_entry_get_userdata(entry);
}

void wxListBox::DoSetItemClientData(unsigned int n, void* clientData)
{
    wxTreeEntry* entry = GTKGetEntry(n);
    wxCHECK_RET(entry, wxT("could not get entry"));

    wx_tree_entry_set_userdata(entry, clientData);
}

// ----------------------------------------------------------------------------
// string list access
// ----------------------------------------------------------------------------

void wxListBox::SetString(unsigned int n, const wxString& label)
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    GtkTreeIter iter;
    wxCHECK_RET(GTKGetIteratorFor(n, &iter), "invalid index");
    wxTreeEntry* entry = GetEntry(m_liststore, &iter, this);

    // update the item itself
    wx_tree_entry_set_label(entry, wxGTK_CONV(label));

    // signal row changed
    GtkTreeModel* tree_model = GTK_TREE_MODEL(m_liststore);
    wxGtkTreePath path(gtk_tree_model_get_path(tree_model, &iter));
    gtk_tree_model_row_changed(tree_model, path, &iter);
}

wxString wxListBox::GetString(unsigned int n) const
{
    wxCHECK_MSG( m_treeview != NULL, wxEmptyString, wxT("invalid listbox") );

    wxTreeEntry* entry = GTKGetEntry(n);
    wxCHECK_MSG( entry, wxEmptyString, wxT("wrong listbox index") );

    return wxGTK_CONV_BACK(wx_tree_entry_get_label(entry));
}

unsigned int wxListBox::GetCount() const
{
    wxCHECK_MSG( m_treeview != NULL, 0, wxT("invalid listbox") );

    return (unsigned int)gtk_tree_model_iter_n_children(GTK_TREE_MODEL(m_liststore), NULL);
}

int wxListBox::FindString( const wxString &item, bool bCase ) const
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox") );

    //Sort of hackish - maybe there is a faster way
    unsigned int nCount = wxListBox::GetCount();

    for(unsigned int i = 0; i < nCount; ++i)
    {
        if( item.IsSameAs( wxListBox::GetString(i), bCase ) )
            return (int)i;
    }


    // it's not an error if the string is not found -> no wxCHECK
    return wxNOT_FOUND;
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

void wxListBox::GTKOnActivated(int item)
{
    SendEvent(wxEVT_LISTBOX_DCLICK, item, IsSelected(item));
}

void wxListBox::GTKOnSelectionChanged()
{
    if ( HasFlag(wxLB_MULTIPLE | wxLB_EXTENDED) )
    {
        CalcAndSendEvent();
    }
    else // single selection
    {
        const int item = GetSelection();
        if (item >= 0 && DoChangeSingleSelection(item))
            SendEvent(wxEVT_LISTBOX, item, true);
    }
}

int wxListBox::GetSelection() const
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox"));
    wxCHECK_MSG( HasFlag(wxLB_SINGLE), wxNOT_FOUND,
                    wxT("must be single selection listbox"));

    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    // only works on single-sel
    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return wxNOT_FOUND;

    return GTKGetIndexFor(iter);
}

int wxListBox::GetSelections( wxArrayInt& aSelections ) const
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox") );

    aSelections.Empty();

    int i = 0;
    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_liststore), &iter))
    { //gtk_tree_selection_get_selected_rows is GTK 2.2+ so iter instead
        do
        {
            if (gtk_tree_selection_iter_is_selected(selection, &iter))
                aSelections.Add(i);

            i++;
        } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(m_liststore), &iter));
    }

    return aSelections.GetCount();
}

bool wxListBox::IsSelected( int n ) const
{
    wxCHECK_MSG( m_treeview != NULL, false, wxT("invalid listbox") );

    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    GtkTreeIter iter;
    wxCHECK_MSG( GTKGetIteratorFor(n, &iter), false, wxT("Invalid index") );

    return gtk_tree_selection_iter_is_selected(selection, &iter) != 0;
}

void wxListBox::DoSetSelection( int n, bool select )
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    wxGtkEventsDisabler<wxListBox> noEvents(this);

    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    // passing -1 to SetSelection() is documented to deselect all items
    if ( n == wxNOT_FOUND )
    {
        gtk_tree_selection_unselect_all(selection);
        return;
    }

    wxCHECK_RET( IsValid(n), wxT("invalid index in wxListBox::SetSelection") );


    GtkTreeIter iter;
    wxCHECK_RET( GTKGetIteratorFor(n, &iter), wxT("Invalid index") );

    if (select)
        gtk_tree_selection_select_iter(selection, &iter);
    else
        gtk_tree_selection_unselect_iter(selection, &iter);

    wxGtkTreePath path(
            gtk_tree_model_get_path(GTK_TREE_MODEL(m_liststore), &iter));

    gtk_tree_view_scroll_to_cell(m_treeview, path, NULL, FALSE, 0.0f, 0.0f);
}

void wxListBox::DoScrollToCell(int n, float alignY, float alignX)
{
    wxCHECK_RET( m_treeview, wxT("invalid listbox") );
    wxCHECK_RET( IsValid(n), wxT("invalid index"));

    //RN: I have no idea why this line is needed...
    if (gtk_widget_has_grab(GTK_WIDGET(m_treeview)))
        return;

    GtkTreeIter iter;
    if ( !GTKGetIteratorFor(n, &iter) )
        return;

    wxGtkTreePath path(
            gtk_tree_model_get_path(GTK_TREE_MODEL(m_liststore), &iter));

    // Scroll to the desired cell (0.0 == topleft alignment)
    gtk_tree_view_scroll_to_cell(m_treeview, path, NULL,
                                 TRUE, alignY, alignX);
}

void wxListBox::DoSetFirstItem(int n)
{
    DoScrollToCell(n, 0, 0);
}

void wxListBox::EnsureVisible(int n)
{
    DoScrollToCell(n, 0.5, 0);
}

int wxListBox::GetTopItem() const
{
    int idx = wxNOT_FOUND;

    wxGtkTreePath start;
    if ( gtk_tree_view_get_visible_range(m_treeview, start.ByRef(), NULL) )
    {
        gint *ptr = gtk_tree_path_get_indices(start);

        if ( ptr )
            idx = *ptr;
    }

    return idx;
}

// ----------------------------------------------------------------------------
// hittest
// ----------------------------------------------------------------------------

int wxListBox::DoListHitTest(const wxPoint& point) const
{
    // gtk_tree_view_get_path_at_pos() also gets items that are not visible and
    // we only want visible items we need to check for it manually here
    if ( !GetClientRect().Contains(point) )
        return wxNOT_FOUND;

    // need to translate from master window since it is in client coords
    gint binx, biny;
    gdk_window_get_geometry(gtk_tree_view_get_bin_window(m_treeview),
                            &binx, &biny, NULL, NULL);

    wxGtkTreePath path;
    if ( !gtk_tree_view_get_path_at_pos
          (
            m_treeview,
            point.x - binx,
            point.y - biny,
            path.ByRef(),
            NULL,   // [out] column (always 0 here)
            NULL,   // [out] x-coord relative to the cell (not interested)
            NULL    // [out] y-coord relative to the cell
          ) )
    {
        return wxNOT_FOUND;
    }

    return gtk_tree_path_get_indices(path)[0];
}

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

GtkWidget *wxListBox::GetConnectWidget()
{
    // the correct widget for listbox events (such as mouse clicks for example)
    // is m_treeview, not the parent scrolled window
    return GTK_WIDGET(m_treeview);
}

GdkWindow *wxListBox::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return gtk_tree_view_get_bin_window(m_treeview);
}

void wxListBox::DoApplyWidgetStyle(GtkRcStyle *style)
{
#ifdef __WXGTK3__
    // don't know if this is even necessary, or how to do it
#else
    if (m_hasBgCol && m_backgroundColour.IsOk())
    {
        GdkWindow *window = gtk_tree_view_get_bin_window(m_treeview);
        if (window)
        {
            m_backgroundColour.CalcPixel( gdk_drawable_get_colormap( window ) );
            gdk_window_set_background( window, m_backgroundColour.GetColor() );
            gdk_window_clear( window );
        }
    }
#endif

    GTKApplyStyle(GTK_WIDGET(m_treeview), style);
}

wxSize wxListBox::DoGetBestSize() const
{
    wxCHECK_MSG(m_treeview, wxDefaultSize, wxT("invalid tree view"));

    // Start with a minimum size that's not too small
    int cx, cy;
    GetTextExtent( wxT("X"), &cx, &cy);
    int lbWidth = 0;
    int lbHeight = 10;

    // Find the widest string.
    const unsigned int count = GetCount();
    if ( count )
    {
        int wLine;
        for ( unsigned int i = 0; i < count; i++ )
        {
            GetTextExtent(GetString(i), &wLine, NULL);
            if ( wLine > lbWidth )
                lbWidth = wLine;
        }
    }

    lbWidth += 3 * cx;

    // And just a bit more for the checkbox if present and then some
    // (these are rough guesses)
#if wxUSE_CHECKLISTBOX
    if ( m_hasCheckBoxes )
    {
        lbWidth += 35;
        cy = cy > 25 ? cy : 25; // rough height of checkbox
    }
#endif

    // Add room for the scrollbar
    lbWidth += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

    // Don't make the listbox too tall but don't make it too small neither
    lbHeight = (cy+4) * wxMin(wxMax(count, 3), 10);

    wxSize best(lbWidth, lbHeight);
    CacheBestSize(best);
    return best;
}

// static
wxVisualAttributes
wxListBox::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_tree_view_new(), true);
}

#endif // wxUSE_LISTBOX
