/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/radiobut.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: radiobut.cpp 67326 2011-03-28 06:27:49Z PC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_RADIOBTN

#include "wx/radiobut.h"

#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool           g_blockEventsOnDrag;

//-----------------------------------------------------------------------------
// "clicked"
//-----------------------------------------------------------------------------

extern "C" {
static
void gtk_radiobutton_clicked_callback( GtkToggleButton *button, wxRadioButton *rb )
{
    if (!rb->m_hasVMT) return;

    if (g_blockEventsOnDrag) return;

    if (!gtk_toggle_button_get_active(button)) return;

    wxCommandEvent event( wxEVT_COMMAND_RADIOBUTTON_SELECTED, rb->GetId());
    event.SetInt( rb->GetValue() );
    event.SetEventObject( rb );
    rb->HandleWindowEvent( event );
}
}

//-----------------------------------------------------------------------------
// wxRadioButton
//-----------------------------------------------------------------------------

bool wxRadioButton::Create( wxWindow *parent,
                            wxWindowID id,
                            const wxString& label,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxValidator& validator,
                            const wxString& name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxRadioButton creation failed") );
        return false;
    }

    GSList* radioButtonGroup = NULL;
    if (!HasFlag(wxRB_GROUP) && !HasFlag(wxRB_SINGLE))
    {
        // search backward for last group start
        wxWindowList::compatibility_iterator node = parent->GetChildren().GetLast();
        for (; node; node = node->GetPrevious())
        {
            wxWindow *child = node->GetData();
            if (child->HasFlag(wxRB_GROUP) && wxIsKindOf(child, wxRadioButton))
            {
                radioButtonGroup = gtk_radio_button_get_group(
                    GTK_RADIO_BUTTON(child->m_widget));
                break;
            }
        }
    }

    m_widget = gtk_radio_button_new_with_label( radioButtonGroup, wxGTK_CONV( label ) );
    g_object_ref(m_widget);

    SetLabel(label);

    g_signal_connect_after (m_widget, "clicked",
                            G_CALLBACK (gtk_radiobutton_clicked_callback), this);

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

void wxRadioButton::SetLabel( const wxString& label )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid radiobutton") );

    // save the original label
    wxControlBase::SetLabel(label);

    GTKSetLabelForLabel(GTK_LABEL(gtk_bin_get_child(GTK_BIN(m_widget))), label);
}

void wxRadioButton::SetValue( bool val )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid radiobutton") );

    if (val == GetValue())
        return;

    g_signal_handlers_block_by_func(
        m_widget, (void*)gtk_radiobutton_clicked_callback, this);

    if (val)
    {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m_widget), TRUE );
    }
    else
    {
        // should give an assert
        // RL - No it shouldn't.  A wxGenericValidator might try to set it
        //      as FALSE.  Failing silently is probably TRTTD here.
    }

    g_signal_handlers_unblock_by_func(
        m_widget, (void*)gtk_radiobutton_clicked_callback, this);
}

bool wxRadioButton::GetValue() const
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid radiobutton") );

    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_widget)) != 0;
}

bool wxRadioButton::Enable( bool enable )
{
    if (!base_type::Enable(enable))
        return false;

    gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(m_widget)), enable);

    if (enable)
        GTKFixSensitivity();

    return true;
}

void wxRadioButton::DoApplyWidgetStyle(GtkRcStyle *style)
{
    gtk_widget_modify_style(m_widget, style);
    gtk_widget_modify_style(gtk_bin_get_child(GTK_BIN(m_widget)), style);
}

GdkWindow *
wxRadioButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_BUTTON(m_widget)->event_window;
}

// static
wxVisualAttributes
wxRadioButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
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


#endif
