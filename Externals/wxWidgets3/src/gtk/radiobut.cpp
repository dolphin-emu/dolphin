/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/radiobut.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_RADIOBTN

#include "wx/radiobut.h"

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"

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
    if (g_blockEventsOnDrag) return;

    if (!gtk_toggle_button_get_active(button)) return;

    wxCommandEvent event( wxEVT_RADIOBUTTON, rb->GetId());
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

    // Check if this radio button should be put into an existing group. This
    // shouldn't be done if it's given a style to explicitly start a new group
    // or if it's not meant to be a part of a group at all.
    GSList* radioButtonGroup = NULL;
    if (!HasFlag(wxRB_GROUP) && !HasFlag(wxRB_SINGLE))
    {
        // search backward for last group start
        wxWindowList::compatibility_iterator node = parent->GetChildren().GetLast();
        for (; node; node = node->GetPrevious())
        {
            wxWindow *child = node->GetData();

            // We stop at the first previous radio button in any case as it
            // wouldn't make sense to put this button in a group with another
            // one if there is a radio button that is not part of the same
            // group between them.
            if (wxIsKindOf(child, wxRadioButton))
            {
                // Any preceding radio button can be used to get its group, not
                // necessarily one with wxRB_GROUP style, but exclude
                // wxRB_SINGLE ones as their group should never be shared.
                if (!child->HasFlag(wxRB_SINGLE))
                {
                    radioButtonGroup = gtk_radio_button_get_group(
                        GTK_RADIO_BUTTON(child->m_widget));
                }

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
    GTKApplyStyle(m_widget, style);
    GTKApplyStyle(gtk_bin_get_child(GTK_BIN(m_widget)), style);
}

GdkWindow *
wxRadioButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return gtk_button_get_event_window(GTK_BUTTON(m_widget));
}

// static
wxVisualAttributes
wxRadioButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_radio_button_new_with_label(NULL, ""));
}


#endif
