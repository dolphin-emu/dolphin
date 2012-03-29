/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/radiobox.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: radiobox.cpp 67331 2011-03-29 05:15:54Z PC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_RADIOBOX

#include "wx/radiobox.h"

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif

#include "wx/gtk/private.h"

#include <gdk/gdkkeysyms.h>
#if GTK_CHECK_VERSION(3,0,0)
#include <gdk/gdkkeysyms-compat.h>
#endif

//-----------------------------------------------------------------------------
// wxGTKRadioButtonInfo
//-----------------------------------------------------------------------------
// structure internally used by wxRadioBox to store its child buttons

class wxGTKRadioButtonInfo : public wxObject
{
public:
    wxGTKRadioButtonInfo( GtkRadioButton * abutton, const wxRect & arect )
    : button( abutton ), rect( arect ) {}

    GtkRadioButton * button;
    wxRect           rect;
};

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

#include "wx/listimpl.cpp"
WX_DEFINE_LIST( wxRadioBoxButtonsInfoList )

extern bool          g_blockEventsOnDrag;

//-----------------------------------------------------------------------------
// "clicked"
//-----------------------------------------------------------------------------

extern "C" {
static void gtk_radiobutton_clicked_callback( GtkToggleButton *button, wxRadioBox *rb )
{
    if (!rb->m_hasVMT) return;
    if (g_blockEventsOnDrag) return;

    if (!gtk_toggle_button_get_active(button)) return;

    wxCommandEvent event( wxEVT_COMMAND_RADIOBOX_SELECTED, rb->GetId() );
    event.SetInt( rb->GetSelection() );
    event.SetString( rb->GetStringSelection() );
    event.SetEventObject( rb );
    rb->HandleWindowEvent(event);
}
}

//-----------------------------------------------------------------------------
// "key_press_event"
//-----------------------------------------------------------------------------

extern "C" {
static gint gtk_radiobox_keypress_callback( GtkWidget *widget, GdkEventKey *gdk_event, wxRadioBox *rb )
{
    if (!rb->m_hasVMT) return FALSE;
    if (g_blockEventsOnDrag) return FALSE;

    if ( ((gdk_event->keyval == GDK_Tab) ||
          (gdk_event->keyval == GDK_ISO_Left_Tab)) &&
         rb->GetParent() && (rb->GetParent()->HasFlag( wxTAB_TRAVERSAL)) )
    {
        wxNavigationKeyEvent new_event;
        new_event.SetEventObject( rb->GetParent() );
        // GDK reports GDK_ISO_Left_Tab for SHIFT-TAB
        new_event.SetDirection( (gdk_event->keyval == GDK_Tab) );
        // CTRL-TAB changes the (parent) window, i.e. switch notebook page
        new_event.SetWindowChange( (gdk_event->state & GDK_CONTROL_MASK) );
        new_event.SetCurrentFocus( rb );
        return rb->GetParent()->HandleWindowEvent(new_event);
    }

    if ((gdk_event->keyval != GDK_Up) &&
        (gdk_event->keyval != GDK_Down) &&
        (gdk_event->keyval != GDK_Left) &&
        (gdk_event->keyval != GDK_Right))
    {
        return FALSE;
    }

    wxRadioBoxButtonsInfoList::compatibility_iterator node = rb->m_buttonsInfo.GetFirst();
    while( node && GTK_WIDGET( node->GetData()->button ) != widget )
    {
        node = node->GetNext();
    }
    if (!node)
    {
        return FALSE;
    }

    if ((gdk_event->keyval == GDK_Up) ||
        (gdk_event->keyval == GDK_Left))
    {
        if (node == rb->m_buttonsInfo.GetFirst())
            node = rb->m_buttonsInfo.GetLast();
        else
            node = node->GetPrevious();
    }
    else
    {
        if (node == rb->m_buttonsInfo.GetLast())
            node = rb->m_buttonsInfo.GetFirst();
        else
            node = node->GetNext();
    }

    GtkWidget *button = (GtkWidget*) node->GetData()->button;

    gtk_widget_grab_focus( button );

    return TRUE;
}
}

extern "C" {
static gint gtk_radiobutton_focus_out( GtkWidget * WXUNUSED(widget),
                                       GdkEventFocus *WXUNUSED(event),
                                       wxRadioBox *win )
{
    // NB: This control is composed of several GtkRadioButton widgets and
    //     when focus changes from one of them to another in the same
    //     wxRadioBox, we get a focus-out event followed by focus-in for
    //     another GtkRadioButton owned by the same control. We don't want
    //     to generate two spurious wxEVT_SET_FOCUS events in this case,
    //     so we defer sending wx events until idle time.
    win->GTKHandleFocusOut();

    // never stop the signal emission, it seems to break the kbd handling
    // inside the radiobox
    return FALSE;
}
}

extern "C" {
static gint gtk_radiobutton_focus_in( GtkWidget * WXUNUSED(widget),
                                      GdkEventFocus *WXUNUSED(event),
                                      wxRadioBox *win )
{
    win->GTKHandleFocusIn();

    // never stop the signal emission, it seems to break the kbd handling
    // inside the radiobox
    return FALSE;
}
}

extern "C" {
static void gtk_radiobutton_size_allocate( GtkWidget *widget,
                                           GtkAllocation * alloc,
                                           wxRadioBox *win )
{
    for ( wxRadioBoxButtonsInfoList::compatibility_iterator node = win->m_buttonsInfo.GetFirst();
          node;
          node = node->GetNext())
    {
        if (widget == GTK_WIDGET(node->GetData()->button))
        {
            const wxPoint origin = win->GetPosition();
            wxRect rect = wxRect( alloc->x - origin.x, alloc->y - origin.y,
                                  alloc->width, alloc->height );
            node->GetData()->rect = rect;
            break;
        }
    }
}
}


//-----------------------------------------------------------------------------
// wxRadioBox
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxRadioBox,wxControl)

bool wxRadioBox::Create( wxWindow *parent, wxWindowID id,
                         const wxString& title,
                         const wxPoint &pos, const wxSize &size,
                         const wxArrayString& choices, int majorDim,
                         long style, const wxValidator& validator,
                         const wxString &name )
{
    wxCArrayString chs(choices);

    return Create( parent, id, title, pos, size, chs.GetCount(),
                   chs.GetStrings(), majorDim, style, validator, name );
}

bool wxRadioBox::Create( wxWindow *parent, wxWindowID id, const wxString& title,
                         const wxPoint &pos, const wxSize &size,
                         int n, const wxString choices[], int majorDim,
                         long style, const wxValidator& validator,
                         const wxString &name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxRadioBox creation failed") );
        return false;
    }

    m_widget = GTKCreateFrame(title);
    g_object_ref(m_widget);
    wxControl::SetLabel(title);
    if ( HasFlag(wxNO_BORDER) )
    {
        // If we don't do this here, the wxNO_BORDER style is ignored in Show()
        gtk_frame_set_shadow_type(GTK_FRAME(m_widget), GTK_SHADOW_NONE);
    }


    // majorDim may be 0 if all trailing parameters were omitted, so don't
    // assert here but just use the correct value for it
    SetMajorDim(majorDim == 0 ? n : majorDim, style);


    unsigned int num_of_cols = GetColumnCount();
    unsigned int num_of_rows = GetRowCount();

    GtkRadioButton *rbtn = NULL;

    GtkWidget *table = gtk_table_new( num_of_rows, num_of_cols, FALSE );
    gtk_table_set_col_spacings( GTK_TABLE(table), 1 );
    gtk_table_set_row_spacings( GTK_TABLE(table), 1 );
    gtk_widget_show( table );
    gtk_container_add( GTK_CONTAINER(m_widget), table );

    wxString label;
    GSList *radio_button_group = NULL;
    for (unsigned int i = 0; i < (unsigned int)n; i++)
    {
        if ( i != 0 )
            radio_button_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON(rbtn) );

        label.Empty();
        for ( wxString::const_iterator pc = choices[i].begin();
              pc != choices[i].end(); ++pc )
        {
            if ( *pc != wxT('&') )
                label += *pc;
        }

        rbtn = GTK_RADIO_BUTTON( gtk_radio_button_new_with_label( radio_button_group, wxGTK_CONV( label ) ) );
        gtk_widget_show( GTK_WIDGET(rbtn) );

        g_signal_connect (rbtn, "key_press_event",
                          G_CALLBACK (gtk_radiobox_keypress_callback), this);

        m_buttonsInfo.Append( new wxGTKRadioButtonInfo( rbtn, wxRect() ) );

        if (HasFlag(wxRA_SPECIFY_COLS))
        {
            int left = i%num_of_cols;
            int right = (i%num_of_cols) + 1;
            int top = i/num_of_cols;
            int bottom = (i/num_of_cols)+1;
            gtk_table_attach( GTK_TABLE(table), GTK_WIDGET(rbtn), left, right, top, bottom,
                  GTK_FILL, GTK_FILL, 1, 1 );
        }
        else
        {
            int left = i/num_of_rows;
            int right = (i/num_of_rows) + 1;
            int top = i%num_of_rows;
            int bottom = (i%num_of_rows)+1;
            gtk_table_attach( GTK_TABLE(table), GTK_WIDGET(rbtn), left, right, top, bottom,
                  GTK_FILL, GTK_FILL, 1, 1 );
        }

        ConnectWidget( GTK_WIDGET(rbtn) );

        if (!i)
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(rbtn), TRUE );

        g_signal_connect (rbtn, "clicked",
                          G_CALLBACK (gtk_radiobutton_clicked_callback), this);
        g_signal_connect (rbtn, "focus_in_event",
                          G_CALLBACK (gtk_radiobutton_focus_in), this);
        g_signal_connect (rbtn, "focus_out_event",
                          G_CALLBACK (gtk_radiobutton_focus_out), this);
        g_signal_connect (rbtn, "size_allocate",
                          G_CALLBACK (gtk_radiobutton_size_allocate), this);
    }

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

wxRadioBox::~wxRadioBox()
{
    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        GtkWidget *button = GTK_WIDGET( node->GetData()->button );
        gtk_widget_destroy( button );
        node = node->GetNext();
    }
    WX_CLEAR_LIST( wxRadioBoxButtonsInfoList, m_buttonsInfo );
}

bool wxRadioBox::Show( bool show )
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid radiobox") );

    if (!wxControl::Show(show))
    {
        // nothing to do
        return false;
    }

    if ( HasFlag(wxNO_BORDER) )
        gtk_widget_hide( m_widget );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        GtkWidget *button = GTK_WIDGET( node->GetData()->button );

        if (show)
            gtk_widget_show( button );
        else
            gtk_widget_hide( button );

        node = node->GetNext();
    }

    return true;
}

void wxRadioBox::SetSelection( int n )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( n );

    wxCHECK_RET( node, wxT("radiobox wrong index") );

    GtkToggleButton *button = GTK_TOGGLE_BUTTON( node->GetData()->button );

    GtkDisableEvents();

    gtk_toggle_button_set_active( button, 1 );

    GtkEnableEvents();
}

int wxRadioBox::GetSelection(void) const
{
    wxCHECK_MSG( m_widget != NULL, wxNOT_FOUND, wxT("invalid radiobox") );

    int count = 0;

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        GtkToggleButton *button = GTK_TOGGLE_BUTTON( node->GetData()->button );
        if (gtk_toggle_button_get_active(button)) return count;
        count++;
        node = node->GetNext();
    }

    wxFAIL_MSG( wxT("wxRadioBox none selected") );

    return wxNOT_FOUND;
}

wxString wxRadioBox::GetString(unsigned int n) const
{
    wxCHECK_MSG( m_widget != NULL, wxEmptyString, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( n );

    wxCHECK_MSG( node, wxEmptyString, wxT("radiobox wrong index") );

    GtkLabel* label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(node->GetData()->button)));

    wxString str( wxGTK_CONV_BACK( gtk_label_get_text(label) ) );

    return str;
}

void wxRadioBox::SetLabel( const wxString& label )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid radiobox") );

    GTKSetLabelForFrame(GTK_FRAME(m_widget), label);
}

void wxRadioBox::SetString(unsigned int item, const wxString& label)
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( item );

    wxCHECK_RET( node, wxT("radiobox wrong index") );

    GtkLabel* g_label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(node->GetData()->button)));

    gtk_label_set_text( g_label, wxGTK_CONV( label ) );
}

bool wxRadioBox::Enable( bool enable )
{
    if ( !wxControl::Enable( enable ) )
        return false;

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        GtkButton *button = GTK_BUTTON( node->GetData()->button );
        GtkLabel *label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(button)));

        gtk_widget_set_sensitive( GTK_WIDGET(button), enable );
        gtk_widget_set_sensitive( GTK_WIDGET(label), enable );
        node = node->GetNext();
    }

    if (enable)
        GTKFixSensitivity();

    return true;
}

bool wxRadioBox::Enable(unsigned int item, bool enable)
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( item );

    wxCHECK_MSG( node, false, wxT("radiobox wrong index") );

    GtkButton *button = GTK_BUTTON( node->GetData()->button );
    GtkLabel *label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(button)));

    gtk_widget_set_sensitive( GTK_WIDGET(button), enable );
    gtk_widget_set_sensitive( GTK_WIDGET(label), enable );

    return true;
}

bool wxRadioBox::IsItemEnabled(unsigned int item) const
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( item );

    wxCHECK_MSG( node, false, wxT("radiobox wrong index") );

    GtkButton *button = GTK_BUTTON( node->GetData()->button );

    // don't use GTK_WIDGET_IS_SENSITIVE() here, we want to return true even if
    // the parent radiobox is disabled
    return gtk_widget_get_sensitive(GTK_WIDGET(button));
}

bool wxRadioBox::Show(unsigned int item, bool show)
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( item );

    wxCHECK_MSG( node, false, wxT("radiobox wrong index") );

    GtkWidget *button = GTK_WIDGET( node->GetData()->button );

    if (show)
        gtk_widget_show( button );
    else
        gtk_widget_hide( button );

    return true;
}

bool wxRadioBox::IsItemShown(unsigned int item) const
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid radiobox") );

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.Item( item );

    wxCHECK_MSG( node, false, wxT("radiobox wrong index") );

    GtkButton *button = GTK_BUTTON( node->GetData()->button );

    return gtk_widget_get_visible(GTK_WIDGET(button));
}

unsigned int wxRadioBox::GetCount() const
{
    return m_buttonsInfo.GetCount();
}

void wxRadioBox::GtkDisableEvents()
{
    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        g_signal_handlers_block_by_func(node->GetData()->button,
            (gpointer)gtk_radiobutton_clicked_callback, this);

        node = node->GetNext();
    }
}

void wxRadioBox::GtkEnableEvents()
{
    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        g_signal_handlers_unblock_by_func(node->GetData()->button,
            (gpointer)gtk_radiobutton_clicked_callback, this);

        node = node->GetNext();
    }
}

void wxRadioBox::DoApplyWidgetStyle(GtkRcStyle *style)
{
    GTKFrameApplyWidgetStyle(GTK_FRAME(m_widget), style);

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        GtkWidget *widget = GTK_WIDGET( node->GetData()->button );

        gtk_widget_modify_style( widget, style );
        gtk_widget_modify_style(gtk_bin_get_child(GTK_BIN(widget)), style);

        node = node->GetNext();
    }
}

bool wxRadioBox::GTKWidgetNeedsMnemonic() const
{
    return true;
}

void wxRadioBox::GTKWidgetDoSetMnemonic(GtkWidget* w)
{
    GTKFrameSetMnemonicWidget(GTK_FRAME(m_widget), w);
}

#if wxUSE_TOOLTIPS
void wxRadioBox::GTKApplyToolTip(const char* tip)
{
    // set this tooltip for all radiobuttons which don't have their own tips
    unsigned n = 0;
    for ( wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
          node;
          node = node->GetNext(), n++ )
    {
        if ( !GetItemToolTip(n) )
        {
            wxToolTip::GTKApply(GTK_WIDGET(node->GetData()->button), tip);
        }
    }
}

void wxRadioBox::DoSetItemToolTip(unsigned int n, wxToolTip *tooltip)
{
    wxCharBuffer buf;
    if ( !tooltip )
        tooltip = GetToolTip();
    if ( tooltip )
        buf = wxGTK_CONV(tooltip->GetTip());

    wxToolTip::GTKApply(GTK_WIDGET(m_buttonsInfo[n]->button), buf);
}

#endif // wxUSE_TOOLTIPS

GdkWindow *wxRadioBox::GTKGetWindow(wxArrayGdkWindows& windows) const
{
    windows.push_back(gtk_widget_get_window(m_widget));

    wxRadioBoxButtonsInfoList::compatibility_iterator node = m_buttonsInfo.GetFirst();
    while (node)
    {
        GtkWidget *button = GTK_WIDGET( node->GetData()->button );

        // don't put NULL pointers in the 'windows' array!
        if (gtk_widget_get_window(button))
            windows.push_back(gtk_widget_get_window(button));

        node = node->GetNext();
    }

    return NULL;
}

// static
wxVisualAttributes
wxRadioBox::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    wxVisualAttributes attr;
    // NB: we need toplevel window so that GTK+ can find the right style
    GtkWidget *wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget* widget = gtk_radio_button_new_with_label(NULL, "");
    gtk_container_add(GTK_CONTAINER(wnd), widget);
    attr = GetDefaultAttributesFromGTKWidget(widget);
    gtk_widget_destroy(wnd);
    return attr;
}

int wxRadioBox::GetItemFromPoint(const wxPoint& point) const
{
    const wxPoint pt = ScreenToClient(point);
    unsigned n = 0;
    for ( wxRadioBoxButtonsInfoList::compatibility_iterator
            node = m_buttonsInfo.GetFirst(); node; node = node->GetNext(), n++ )
    {
        if ( m_buttonsInfo[n]->rect.Contains(pt) )
            return n;
    }

    return wxNOT_FOUND;
}

#endif // wxUSE_RADIOBOX
