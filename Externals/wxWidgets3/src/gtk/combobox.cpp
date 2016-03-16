/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/combobox.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_COMBOBOX

#include "wx/combobox.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/settings.h"
    #include "wx/textctrl.h"    // for wxEVT_TEXT
    #include "wx/arrstr.h"
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"

// ----------------------------------------------------------------------------
// GTK callbacks
// ----------------------------------------------------------------------------

extern "C" {
static void
gtkcombobox_text_changed_callback( GtkWidget *WXUNUSED(widget), wxComboBox *combo )
{
    wxCommandEvent event( wxEVT_TEXT, combo->GetId() );
    event.SetString( combo->GetValue() );
    event.SetEventObject( combo );
    combo->HandleWindowEvent( event );
}

static void
gtkcombobox_changed_callback( GtkWidget *WXUNUSED(widget), wxComboBox *combo )
{
    combo->SendSelectionChangedEvent(wxEVT_COMBOBOX);
}

static void
gtkcombobox_popupshown_callback(GObject *WXUNUSED(gobject),
                                GParamSpec *WXUNUSED(param_spec),
                                wxComboBox *combo)
{
    gboolean isShown;
    g_object_get( combo->m_widget, "popup-shown", &isShown, NULL );
    wxCommandEvent event( isShown ? wxEVT_COMBOBOX_DROPDOWN
                                  : wxEVT_COMBOBOX_CLOSEUP,
                          combo->GetId() );
    event.SetEventObject( combo );

#ifndef __WXGTK3__
    // Process the close up event once the combobox is already closed with GTK+
    // 2, otherwise changing the combobox from its handler result in errors.
    if ( !isShown )
    {
        combo->GetEventHandler()->AddPendingEvent( event );
    }
    else
#endif // GTK+ < 3
    {
        combo->HandleWindowEvent( event );
    }
}

}

//-----------------------------------------------------------------------------
// wxComboBox
//-----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxComboBox, wxChoice)
    EVT_CHAR(wxComboBox::OnChar)

    EVT_MENU(wxID_CUT, wxComboBox::OnCut)
    EVT_MENU(wxID_COPY, wxComboBox::OnCopy)
    EVT_MENU(wxID_PASTE, wxComboBox::OnPaste)
    EVT_MENU(wxID_UNDO, wxComboBox::OnUndo)
    EVT_MENU(wxID_REDO, wxComboBox::OnRedo)
    EVT_MENU(wxID_CLEAR, wxComboBox::OnDelete)
    EVT_MENU(wxID_SELECTALL, wxComboBox::OnSelectAll)

    EVT_UPDATE_UI(wxID_CUT, wxComboBox::OnUpdateCut)
    EVT_UPDATE_UI(wxID_COPY, wxComboBox::OnUpdateCopy)
    EVT_UPDATE_UI(wxID_PASTE, wxComboBox::OnUpdatePaste)
    EVT_UPDATE_UI(wxID_UNDO, wxComboBox::OnUpdateUndo)
    EVT_UPDATE_UI(wxID_REDO, wxComboBox::OnUpdateRedo)
    EVT_UPDATE_UI(wxID_CLEAR, wxComboBox::OnUpdateDelete)
    EVT_UPDATE_UI(wxID_SELECTALL, wxComboBox::OnUpdateSelectAll)
wxEND_EVENT_TABLE()

wxComboBox::~wxComboBox()
{
    if (m_entry)
        GTKDisconnect(m_entry);
}

void wxComboBox::Init()
{
    m_entry = NULL;
}

bool wxComboBox::Create( wxWindow *parent, wxWindowID id,
                         const wxString& value,
                         const wxPoint& pos, const wxSize& size,
                         const wxArrayString& choices,
                         long style, const wxValidator& validator,
                         const wxString& name )
{
    wxCArrayString chs(choices);

    return Create( parent, id, value, pos, size, chs.GetCount(),
                   chs.GetStrings(), style, validator, name );
}

bool wxComboBox::Create( wxWindow *parent, wxWindowID id, const wxString& value,
                         const wxPoint& pos, const wxSize& size,
                         int n, const wxString choices[],
                         long style, const wxValidator& validator,
                         const wxString& name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxComboBox creation failed") );
        return false;
    }

    if (HasFlag(wxCB_SORT))
        m_strings = new wxGtkCollatedArrayString();

    GTKCreateComboBoxWidget();

    if (HasFlag(wxBORDER_NONE))
    {
        // Doesn't seem to work
        // g_object_set (m_widget, "has-frame", FALSE, NULL);
    }

    GtkEntry * const entry = GetEntry();

    if ( entry )
    {
        // Set it up to trigger default item on enter key press
        gtk_entry_set_activates_default( entry,
                                         !HasFlag(wxTE_PROCESS_ENTER) );

        gtk_editable_set_editable(GTK_EDITABLE(entry), true);
    }

    Append(n, choices);

    m_parent->DoAddChild( this );

    if ( entry )
        m_focusWidget = GTK_WIDGET( entry );

    PostCreation(size);

    if ( entry )
    {
        if (style & wxCB_READONLY)
        {
            // this will assert and do nothing if the value is not in our list
            // of strings which is the desired behaviour (for consistency with
            // wxMSW and also because it doesn't make sense to have a string
            // which is not a possible choice in a read-only combobox)
            SetStringSelection(value);
            gtk_editable_set_editable(GTK_EDITABLE(entry), false);
        }
        else // editable combobox
        {
            // any value is accepted, even if it's not in our list
            gtk_entry_set_text( entry, wxGTK_CONV(value) );
        }

        g_signal_connect_after (entry, "changed",
                                G_CALLBACK (gtkcombobox_text_changed_callback), this);

        GTKConnectInsertTextSignal(entry);
        GTKConnectClipboardSignals(GTK_WIDGET(entry));
    }

    g_signal_connect_after (m_widget, "changed",
                        G_CALLBACK (gtkcombobox_changed_callback), this);

#ifndef __WXGTK3__
    if ( !gtk_check_version(2,10,0) )
#endif
    {
        g_signal_connect (m_widget, "notify::popup-shown",
                          G_CALLBACK (gtkcombobox_popupshown_callback), this);
    }

    return true;
}

void wxComboBox::GTKCreateComboBoxWidget()
{
#ifdef __WXGTK3__
    m_widget = gtk_combo_box_text_new_with_entry();
#else
    m_widget = gtk_combo_box_entry_new_text();
#endif
    g_object_ref(m_widget);

    m_entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(m_widget)));
}

GtkEditable *wxComboBox::GetEditable() const
{
    return GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(m_widget)));
}

void wxComboBox::OnChar( wxKeyEvent &event )
{
    switch ( event.GetKeyCode() )
    {
        case WXK_RETURN:
            if ( HasFlag(wxTE_PROCESS_ENTER) && GetEntry() )
            {
                // GTK automatically selects an item if its in the list
                wxCommandEvent eventEnter(wxEVT_TEXT_ENTER, GetId());
                eventEnter.SetString( GetValue() );
                eventEnter.SetInt( GetSelection() );
                eventEnter.SetEventObject( this );

                if ( HandleWindowEvent(eventEnter) )
                {
                    // Catch GTK event so that GTK doesn't open the drop
                    // down list upon RETURN.
                    return;
                }
            }
            break;
    }

    event.Skip();
}

void wxComboBox::EnableTextChangedEvents(bool enable)
{
    if ( !GetEntry() )
        return;

    if ( enable )
    {
        g_signal_handlers_unblock_by_func(gtk_bin_get_child(GTK_BIN(m_widget)),
            (gpointer)gtkcombobox_text_changed_callback, this);
    }
    else // disable
    {
        g_signal_handlers_block_by_func(gtk_bin_get_child(GTK_BIN(m_widget)),
            (gpointer)gtkcombobox_text_changed_callback, this);
    }
}

void wxComboBox::GTKDisableEvents()
{
    EnableTextChangedEvents(false);

    g_signal_handlers_block_by_func(m_widget,
        (gpointer)gtkcombobox_changed_callback, this);
    g_signal_handlers_block_by_func(m_widget,
        (gpointer)gtkcombobox_popupshown_callback, this);
}

void wxComboBox::GTKEnableEvents()
{
    EnableTextChangedEvents(true);

    g_signal_handlers_unblock_by_func(m_widget,
        (gpointer)gtkcombobox_changed_callback, this);
    g_signal_handlers_unblock_by_func(m_widget,
        (gpointer)gtkcombobox_popupshown_callback, this);
}

GtkWidget* wxComboBox::GetConnectWidget()
{
    return GTK_WIDGET( GetEntry() );
}

GdkWindow* wxComboBox::GTKGetWindow(wxArrayGdkWindows& /* windows */) const
{
#ifdef __WXGTK3__
    return GTKFindWindow(GTK_WIDGET(GetEntry()));
#else
    return gtk_entry_get_text_window(GetEntry());
#endif
}

// static
wxVisualAttributes
wxComboBox::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
#ifdef __WXGTK3__
    return GetDefaultAttributesFromGTKWidget(gtk_combo_box_new_with_entry(), true);
#else
    return GetDefaultAttributesFromGTKWidget(gtk_combo_box_entry_new(), true);
#endif
}

void wxComboBox::Clear()
{
    wxTextEntry::Clear();
    wxItemContainer::Clear();
}

void wxComboBox::SetValue(const wxString& value)
{
    if ( HasFlag(wxCB_READONLY) )
        SetStringSelection(value);
    else
        wxTextEntry::SetValue(value);
}

void wxComboBox::SetString(unsigned int n, const wxString& text)
{
    wxChoice::SetString(n, text);

    if ( static_cast<int>(n) == GetSelection() )
    {
        // We also need to update the currently shown text, for consistency
        // with wxMSW and also because it makes sense as leaving the old string
        // in the text but not in the list would be confusing to the user.
        SetValue(text);

        // And we need to keep the selection unchanged, modifying the item is
        // not supposed to deselect it.
        SetSelection(n);
    }
}

// ----------------------------------------------------------------------------
// standard event handling
// ----------------------------------------------------------------------------

void wxComboBox::OnCut(wxCommandEvent& WXUNUSED(event))
{
    Cut();
}

void wxComboBox::OnCopy(wxCommandEvent& WXUNUSED(event))
{
    Copy();
}

void wxComboBox::OnPaste(wxCommandEvent& WXUNUSED(event))
{
    Paste();
}

void wxComboBox::OnUndo(wxCommandEvent& WXUNUSED(event))
{
    Undo();
}

void wxComboBox::OnRedo(wxCommandEvent& WXUNUSED(event))
{
    Redo();
}

void wxComboBox::OnDelete(wxCommandEvent& WXUNUSED(event))
{
    RemoveSelection();
}

void wxComboBox::OnSelectAll(wxCommandEvent& WXUNUSED(event))
{
    SelectAll();
}

void wxComboBox::OnUpdateCut(wxUpdateUIEvent& event)
{
    event.Enable( CanCut() );
}

void wxComboBox::OnUpdateCopy(wxUpdateUIEvent& event)
{
    event.Enable( CanCopy() );
}

void wxComboBox::OnUpdatePaste(wxUpdateUIEvent& event)
{
    event.Enable( CanPaste() );
}

void wxComboBox::OnUpdateUndo(wxUpdateUIEvent& event)
{
    event.Enable( CanUndo() );
}

void wxComboBox::OnUpdateRedo(wxUpdateUIEvent& event)
{
    event.Enable( CanRedo() );
}

void wxComboBox::OnUpdateDelete(wxUpdateUIEvent& event)
{
    event.Enable(HasSelection() && IsEditable()) ;
}

void wxComboBox::OnUpdateSelectAll(wxUpdateUIEvent& event)
{
    event.Enable(!wxTextEntry::IsEmpty());
}

void wxComboBox::Popup()
{
     gtk_combo_box_popup( GTK_COMBO_BOX(m_widget) );
}

void wxComboBox::Dismiss()
{
    gtk_combo_box_popdown( GTK_COMBO_BOX(m_widget) );
}

wxSize wxComboBox::DoGetSizeFromTextSize(int xlen, int ylen) const
{
    wxSize tsize( wxChoice::DoGetSizeFromTextSize(xlen, ylen) );

    GtkEntry* entry = GetEntry();
    if (entry)
    {
        // Add the margins we have previously set, but only the horizontal border
        // as vertical one has been taken account in the previous call.
        // Also get other GTK+ margins.
        tsize.IncBy(GTKGetEntryMargins(entry).x, 0);
    }

    return tsize;
}

#endif // wxUSE_COMBOBOX
