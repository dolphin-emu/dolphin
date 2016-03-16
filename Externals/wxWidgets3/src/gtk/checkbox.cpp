/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/checkbox.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_CHECKBOX

#include "wx/checkbox.h"

#include <gtk/gtk.h>
#include "wx/gtk/private/gtk2-compat.h"
#include "wx/gtk/private/eventsdisabler.h"

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool           g_blockEventsOnDrag;

//-----------------------------------------------------------------------------
// "clicked"
//-----------------------------------------------------------------------------

extern "C" {
static void gtk_checkbox_toggled_callback(GtkWidget *widget, wxCheckBox *cb)
{
    if (g_blockEventsOnDrag) return;

    // Transitions for 3state checkbox must be done manually, GTK's checkbox
    // is 2state with additional "undetermined state" flag which isn't
    // changed automatically:
    if (cb->Is3State())
    {
        GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(widget);

        if (cb->Is3rdStateAllowedForUser())
        {
            // The 3 states cycle like this when clicked:
            // checked -> undetermined -> unchecked -> checked -> ...
            bool active = gtk_toggle_button_get_active(toggle) != 0;
            bool inconsistent = gtk_toggle_button_get_inconsistent(toggle) != 0;

            wxGtkEventsDisabler<wxCheckBox> noEvents(cb);

            if (!active && !inconsistent)
            {
                // checked -> undetermined
                gtk_toggle_button_set_active(toggle, true);
                gtk_toggle_button_set_inconsistent(toggle, true);
            }
            else if (!active && inconsistent)
            {
                // undetermined -> unchecked
                gtk_toggle_button_set_inconsistent(toggle, false);
            }
            else if (active && !inconsistent)
            {
                // unchecked -> checked
                // nothing to do
            }
            else
            {
                wxFAIL_MSG(wxT("3state wxCheckBox in unexpected state!"));
            }
        }
        else
        {
            // user's action unsets undetermined state:
            gtk_toggle_button_set_inconsistent(toggle, false);
        }
    }

    wxCommandEvent event(wxEVT_CHECKBOX, cb->GetId());
    event.SetInt(cb->Get3StateValue());
    event.SetEventObject(cb);
    cb->HandleWindowEvent(event);
}
}

//-----------------------------------------------------------------------------
// wxCheckBox
//-----------------------------------------------------------------------------

wxCheckBox::wxCheckBox()
{
    m_widgetCheckbox = NULL;
}

wxCheckBox::~wxCheckBox()
{
    if (m_widgetCheckbox && m_widgetCheckbox != m_widget)
        GTKDisconnect(m_widgetCheckbox);
}

bool wxCheckBox::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxString &label,
                        const wxPoint &pos,
                        const wxSize &size,
                        long style,
                        const wxValidator& validator,
                        const wxString &name )
{
    WXValidateStyle( &style );
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxCheckBox creation failed") );
        return false;
    }

    if ( style & wxALIGN_RIGHT )
    {
        // VZ: as I don't know a way to create a right aligned checkbox with
        //     GTK we will create a checkbox without label and a label at the
        //     left of it
        m_widgetCheckbox = gtk_check_button_new();

        m_widgetLabel = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(m_widgetLabel), 0.0, 0.5);

        m_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(m_widget), m_widgetLabel, FALSE, FALSE, 3);
        gtk_box_pack_start(GTK_BOX(m_widget), m_widgetCheckbox, FALSE, FALSE, 3);

        gtk_widget_show( m_widgetLabel );
        gtk_widget_show( m_widgetCheckbox );
    }
    else
    {
        m_widgetCheckbox = gtk_check_button_new_with_label("");
        m_widgetLabel = gtk_bin_get_child(GTK_BIN(m_widgetCheckbox));
        m_widget = m_widgetCheckbox;
    }
    g_object_ref(m_widget);
    SetLabel( label );

    g_signal_connect (m_widgetCheckbox, "toggled",
                      G_CALLBACK (gtk_checkbox_toggled_callback), this);

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

void wxCheckBox::GTKDisableEvents()
{
    g_signal_handlers_block_by_func(m_widgetCheckbox,
        (gpointer) gtk_checkbox_toggled_callback, this);
}

void wxCheckBox::GTKEnableEvents()
{
    g_signal_handlers_unblock_by_func(m_widgetCheckbox,
        (gpointer) gtk_checkbox_toggled_callback, this);
}

void wxCheckBox::SetValue( bool state )
{
    wxCHECK_RET( m_widgetCheckbox != NULL, wxT("invalid checkbox") );

    if (state == GetValue())
        return;

    wxGtkEventsDisabler<wxCheckBox> noEvents(this);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m_widgetCheckbox), state );
}

bool wxCheckBox::GetValue() const
{
    wxCHECK_MSG( m_widgetCheckbox != NULL, false, wxT("invalid checkbox") );

    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_widgetCheckbox)) != 0;
}

void wxCheckBox::DoSet3StateValue(wxCheckBoxState state)
{
    SetValue(state != wxCHK_UNCHECKED);
    gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(m_widgetCheckbox),
                                       state == wxCHK_UNDETERMINED);
}

wxCheckBoxState wxCheckBox::DoGet3StateValue() const
{
    if (gtk_toggle_button_get_inconsistent(GTK_TOGGLE_BUTTON(m_widgetCheckbox)))
    {
        return wxCHK_UNDETERMINED;
    }
    else
    {
        return GetValue() ? wxCHK_CHECKED : wxCHK_UNCHECKED;
    }
}

void wxCheckBox::SetLabel( const wxString& label )
{
    wxCHECK_RET( m_widgetLabel != NULL, wxT("invalid checkbox") );

    // save the label inside m_label in case user calls GetLabel() later
    wxControl::SetLabel(label);

    GTKSetLabelForLabel(GTK_LABEL(m_widgetLabel), label);
}

bool wxCheckBox::Enable( bool enable )
{
    if (!base_type::Enable(enable))
        return false;

    gtk_widget_set_sensitive( m_widgetLabel, enable );

    if (enable)
        GTKFixSensitivity();

    return true;
}

void wxCheckBox::DoApplyWidgetStyle(GtkRcStyle *style)
{
    GTKApplyStyle(m_widgetCheckbox, style);
    GTKApplyStyle(m_widgetLabel, style);
}

GdkWindow *wxCheckBox::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return gtk_button_get_event_window(GTK_BUTTON(m_widgetCheckbox));
}

// static
wxVisualAttributes
wxCheckBox::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_check_button_new());
}

#endif
