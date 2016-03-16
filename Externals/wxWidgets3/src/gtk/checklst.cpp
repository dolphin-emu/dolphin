/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/checklst.cpp
// Purpose:
// Author:      Robert Roebling
// Modified by: Ryan Norton (Native GTK2.0+ checklist)
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_CHECKLISTBOX

#include "wx/checklst.h"

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/treeview.h"

//-----------------------------------------------------------------------------
// "toggled"
//-----------------------------------------------------------------------------
extern "C" {
static void gtk_checklist_toggled(GtkCellRendererToggle * WXUNUSED(renderer),
                                  gchar                 *stringpath,
                                  wxCheckListBox        *listbox)
{
    wxCHECK_RET( listbox->m_treeview != NULL, wxT("invalid listbox") );

    wxGtkTreePath path(stringpath);
    wxCommandEvent new_event( wxEVT_CHECKLISTBOX,
                              listbox->GetId() );
    new_event.SetEventObject( listbox );
    new_event.SetInt( gtk_tree_path_get_indices(path)[0] );
    new_event.SetString( listbox->GetString( new_event.GetInt() ));
    listbox->Check( new_event.GetInt(), !listbox->IsChecked(new_event.GetInt()));
    listbox->HandleWindowEvent( new_event );
}
}

//-----------------------------------------------------------------------------
// wxCheckListBox
//-----------------------------------------------------------------------------

wxCheckListBox::wxCheckListBox() : wxCheckListBoxBase()
{
    m_hasCheckBoxes = true;
}

wxCheckListBox::wxCheckListBox(wxWindow *parent, wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               int nStrings,
                               const wxString *choices,
                               long style,
                               const wxValidator& validator,
                               const wxString& name )
{
    m_hasCheckBoxes = true;
    wxListBox::Create( parent, id, pos, size, nStrings, choices, style, validator, name );
}

wxCheckListBox::wxCheckListBox(wxWindow *parent, wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               const wxArrayString& choices,
                               long style,
                               const wxValidator& validator,
                               const wxString& name )
{
    m_hasCheckBoxes = true;
    wxListBox::Create( parent, id, pos, size, choices,
                       style, validator, name );
}

void wxCheckListBox::DoCreateCheckList()
{
    //Create the checklist in our treeview and set up events for it
    GtkCellRenderer* renderer =
        gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn* column =
        gtk_tree_view_column_new_with_attributes( "", renderer,
                                                  "active", 0,
                                                  NULL );
    gtk_tree_view_column_set_fixed_width(column, 22);

    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_clickable(column, TRUE);

    g_signal_connect (renderer, "toggled",
                      G_CALLBACK (gtk_checklist_toggled),
                      this);

    gtk_tree_view_append_column(m_treeview, column);
}

bool wxCheckListBox::IsChecked(unsigned int index) const
{
    wxCHECK_MSG( m_treeview != NULL, false, wxT("invalid checklistbox") );

    GtkTreeIter iter;
    gboolean res = gtk_tree_model_iter_nth_child(
                        GTK_TREE_MODEL(m_liststore),
                        &iter, NULL, //NULL = parent = get first
                        index
                   );
    if(!res)
        return false;

    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(GTK_TREE_MODEL(m_liststore),
                             &iter,
                             0, //column
                             &value);

    return g_value_get_boolean(&value) != 0;
}

void wxCheckListBox::Check(unsigned int index, bool check)
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid checklistbox") );

    GtkTreeIter iter;
    gboolean res = gtk_tree_model_iter_nth_child(
                        GTK_TREE_MODEL(m_liststore),
                        &iter, NULL, //NULL = parent = get first
                        index
                   );
    if(!res)
        return;

    gtk_list_store_set(m_liststore,
                       &iter,
                       0, //column
                       check ? TRUE : FALSE, -1);
}

int wxCheckListBox::GetItemHeight() const
{
    wxCHECK_MSG( m_treeview != NULL, 0, wxT("invalid listbox"));

    gint height;
    gtk_tree_view_column_cell_get_size(
        gtk_tree_view_get_column(m_treeview, 0),
                                       NULL, NULL, NULL, NULL,
                                       &height);
    return height;
}

#endif //wxUSE_CHECKLISTBOX
