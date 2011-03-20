/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/choice.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: choice.cpp 66555 2011-01-04 08:31:53Z SC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHOICE || wxUSE_COMBOBOX

#include "wx/choice.h"

#ifndef WX_PRECOMP
    #include "wx/arrstr.h"
#endif

#include "wx/gtk/private.h"


// ----------------------------------------------------------------------------
// GTK callbacks
// ----------------------------------------------------------------------------

extern "C" {

static void
gtk_choice_changed_callback( GtkWidget *WXUNUSED(widget), wxChoice *choice )
{
    choice->SendSelectionChangedEvent(wxEVT_COMMAND_CHOICE_SELECTED);
}

}

//-----------------------------------------------------------------------------
// wxChoice
//-----------------------------------------------------------------------------

void wxChoice::Init()
{
    m_strings = NULL;
    m_stringCellIndex = 0;
}

bool wxChoice::Create( wxWindow *parent, wxWindowID id,
                       const wxPoint &pos, const wxSize &size,
                       const wxArrayString& choices,
                       long style, const wxValidator& validator,
                       const wxString &name )
{
    wxCArrayString chs(choices);

    return Create( parent, id, pos, size, chs.GetCount(), chs.GetStrings(),
                   style, validator, name );
}

bool wxChoice::Create( wxWindow *parent, wxWindowID id,
                       const wxPoint &pos, const wxSize &size,
                       int n, const wxString choices[],
                       long style, const wxValidator& validator,
                       const wxString &name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxChoice creation failed") );
        return false;
    }

    if ( IsSorted() )
    {
        // if our m_strings != NULL, Append() will check for it and insert
        // items in the correct order
        m_strings = new wxGtkCollatedArrayString;
    }

    m_widget = gtk_combo_box_new_text();
    g_object_ref(m_widget);

    Append(n, choices);

    m_parent->DoAddChild( this );

    PostCreation(size);

    g_signal_connect_after (m_widget, "changed",
                            G_CALLBACK (gtk_choice_changed_callback), this);

    return true;
}

wxChoice::~wxChoice()
{
    delete m_strings;
}

void wxChoice::SendSelectionChangedEvent(wxEventType evt_type)
{
    if (!m_hasVMT)
        return;

    if (GetSelection() == -1)
        return;

    wxCommandEvent event( evt_type, GetId() );

    int n = GetSelection();
    event.SetInt( n );
    event.SetString( GetStringSelection() );
    event.SetEventObject( this );
    InitCommandEventWithItems( event, n );

    HandleWindowEvent( event );
}

void wxChoice::GTKInsertComboBoxTextItem( unsigned int n, const wxString& text )
{
    gtk_combo_box_insert_text( GTK_COMBO_BOX( m_widget ), n, wxGTK_CONV( text ) );
}

int wxChoice::DoInsertItems(const wxArrayStringsAdapter & items,
                            unsigned int pos,
                            void **clientData, wxClientDataType type)
{
    wxCHECK_MSG( m_widget != NULL, -1, wxT("invalid control") );

    wxASSERT_MSG( !IsSorted() || (pos == GetCount()),
                 wxT("In a sorted choice data could only be appended"));

    const int count = items.GetCount();

    int n = wxNOT_FOUND;

    for ( int i = 0; i < count; ++i )
    {
        n = pos + i;
        // If sorted, use this wxSortedArrayStrings to determine
        // the right insertion point
        if (m_strings)
            n = m_strings->Add(items[i]);

        GTKInsertComboBoxTextItem( n, items[i] );

        m_clientData.Insert( NULL, n );
        AssignNewItemClientData(n, clientData, i, type);
    }

    InvalidateBestSize();

    return n;
}

void wxChoice::DoSetItemClientData(unsigned int n, void* clientData)
{
    m_clientData[n] = clientData;
}

void* wxChoice::DoGetItemClientData(unsigned int n) const
{
    return m_clientData[n];
}

void wxChoice::DoClear()
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid control") );

    GTKDisableEvents();

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    GtkTreeModel* model = gtk_combo_box_get_model( combobox );
    gtk_list_store_clear(GTK_LIST_STORE(model));

    m_clientData.Clear();

    if (m_strings)
        m_strings->Clear();

    GTKEnableEvents();

    InvalidateBestSize();
}

void wxChoice::DoDeleteOneItem(unsigned int n)
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid control") );
    wxCHECK_RET( IsValid(n), wxT("invalid index in wxChoice::Delete") );

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    GtkTreeModel* model = gtk_combo_box_get_model( combobox );
    GtkListStore* store = GTK_LIST_STORE(model);
    GtkTreeIter iter;
    gtk_tree_model_iter_nth_child( model, &iter,
                                   NULL, (gint) n );
    gtk_list_store_remove( store, &iter );

    m_clientData.RemoveAt( n );
    if ( m_strings )
        m_strings->RemoveAt( n );

    InvalidateBestSize();
}

int wxChoice::FindString( const wxString &item, bool bCase ) const
{
    wxCHECK_MSG( m_widget != NULL, wxNOT_FOUND, wxT("invalid control") );

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    GtkTreeModel* model = gtk_combo_box_get_model( combobox );
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first( model, &iter );
    if (!gtk_list_store_iter_is_valid(GTK_LIST_STORE(model), &iter ))
        return -1;
    int count = 0;
    do
    {
        GValue value = { 0, };
        gtk_tree_model_get_value( model, &iter, m_stringCellIndex, &value );
        wxString str = wxGTK_CONV_BACK( g_value_get_string( &value ) );
        g_value_unset( &value );

        if (item.IsSameAs( str, bCase ) )
            return count;

        count++;
    }
    while ( gtk_tree_model_iter_next(model, &iter) );

    return wxNOT_FOUND;
}

int wxChoice::GetSelection() const
{
    return gtk_combo_box_get_active( GTK_COMBO_BOX( m_widget ) );
}

void wxChoice::SetString(unsigned int n, const wxString &text)
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid control") );

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    wxCHECK_RET( IsValid(n), wxT("invalid index") );

    GtkTreeModel *model = gtk_combo_box_get_model( combobox );
    GtkTreeIter iter;
    if (gtk_tree_model_iter_nth_child (model, &iter, NULL, n))
    {
        GValue value = { 0, };
        g_value_init( &value, G_TYPE_STRING );
        g_value_set_string( &value, wxGTK_CONV( text ) );
        gtk_list_store_set_value( GTK_LIST_STORE(model), &iter, m_stringCellIndex, &value );
        g_value_unset( &value );
    }

    InvalidateBestSize();
}

wxString wxChoice::GetString(unsigned int n) const
{
    wxCHECK_MSG( m_widget != NULL, wxEmptyString, wxT("invalid control") );

    wxString str;

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    GtkTreeModel *model = gtk_combo_box_get_model( combobox );
    GtkTreeIter iter;
    if (gtk_tree_model_iter_nth_child (model, &iter, NULL, n))
    {
        GValue value = { 0, };
        gtk_tree_model_get_value( model, &iter, m_stringCellIndex, &value );
        wxString tmp = wxGTK_CONV_BACK( g_value_get_string( &value ) );
        g_value_unset( &value );
        return tmp;
    }

    return str;
}

unsigned int wxChoice::GetCount() const
{
    wxCHECK_MSG( m_widget != NULL, 0, wxT("invalid control") );

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    GtkTreeModel* model = gtk_combo_box_get_model( combobox );
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first( model, &iter );
    if (!gtk_list_store_iter_is_valid(GTK_LIST_STORE(model), &iter ))
        return 0;
    unsigned int ret = 1;
    while (gtk_tree_model_iter_next( model, &iter ))
        ret++;
    return ret;
}

void wxChoice::SetSelection( int n )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid control") );

    GTKDisableEvents();

    GtkComboBox* combobox = GTK_COMBO_BOX( m_widget );
    gtk_combo_box_set_active( combobox, n );

    GTKEnableEvents();
}

void wxChoice::SetColumns(int n)
{
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(m_widget), n);
}

int wxChoice::GetColumns() const
{
    // gtk_combo_box_get_wrap_width() was added in gtk 2.6
    gint intval;
    g_object_get(G_OBJECT(m_widget), "wrap-width", &intval, NULL);
    return intval;
}


void wxChoice::GTKDisableEvents()
{
    g_signal_handlers_block_by_func(m_widget,
                                (gpointer) gtk_choice_changed_callback, this);
}

void wxChoice::GTKEnableEvents()
{
    g_signal_handlers_unblock_by_func(m_widget,
                                (gpointer) gtk_choice_changed_callback, this);
}


GdkWindow *wxChoice::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return m_widget->window;
}

// Notice that this method shouldn't be necessary, because GTK calculates
// properly size of the combobox but for unknown reasons it doesn't work
// correctly in wx without this.
wxSize wxChoice::DoGetBestSize() const
{
    // strangely, this returns a width of 188 pixels from GTK+ (?)
    wxSize ret( wxControl::DoGetBestSize() );

    // we know better our horizontal extent: it depends on the longest string
    // in the combobox
    if ( m_widget )
    {
        ret.x = GetCount() > 0 ? 0 : 60;  // start with something "sensible"
        int width;
        unsigned int count = GetCount();
        for ( unsigned int n = 0; n < count; n++ )
        {
            GetTextExtent(GetString(n), &width, NULL, NULL, NULL );
            if ( width + 40 > ret.x ) // 40 for drop down arrow and space around text
                ret.x = width + 40;
        }
    }

    // empty combobox should have some reasonable default size too
    if ((GetCount() == 0) && (ret.x < 80))
        ret.x = 80;

    CacheBestSize(ret);
    return ret;
}

void wxChoice::DoApplyWidgetStyle(GtkRcStyle *style)
{
    gtk_widget_modify_style(m_widget, style);
    gtk_widget_modify_style(GTK_BIN(m_widget)->child, style);
}


// static
wxVisualAttributes
wxChoice::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_combo_box_new);
}


#endif // wxUSE_CHOICE || wxUSE_COMBOBOX
