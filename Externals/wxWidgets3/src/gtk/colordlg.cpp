/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/colordlg.cpp
// Purpose:     Native wxColourDialog for GTK+
// Author:      Vaclav Slavik
// Modified by:
// Created:     2004/06/04
// RCS-ID:      $Id: colordlg.cpp 67681 2011-05-03 16:29:04Z DS $
// Copyright:   (c) Vaclav Slavik, 2004
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_COLOURDLG

#include "wx/colordlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
#endif

#include "wx/gtk/private.h"

#if wxUSE_LIBHILDON
    #include <hildon-widgets/hildon-color-selector.h>
#endif // wxUSE_LIBHILDON

#if wxUSE_LIBHILDON2
extern "C" {
    #include <hildon/hildon.h>
}
#endif // wxUSE_LIBHILDON2

IMPLEMENT_DYNAMIC_CLASS(wxColourDialog, wxDialog)

wxColourDialog::wxColourDialog(wxWindow *parent, wxColourData *data)
{
    Create(parent, data);
}

bool wxColourDialog::Create(wxWindow *parent, wxColourData *data)
{
    if (data)
        m_data = *data;

    m_parent = GetParentForModalDialog(parent, 0);
    GtkWindow * const parentGTK = m_parent ? GTK_WINDOW(m_parent->m_widget)
                                           : NULL;

#if wxUSE_LIBHILDON
    m_widget = hildon_color_selector_new(parentGTK);
#elif wxUSE_LIBHILDON2 // !wxUSE_LIBHILDON
    m_widget = hildon_color_chooser_dialog_new();
#else // !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    wxString title(_("Choose colour"));
    m_widget = gtk_color_selection_dialog_new(wxGTK_CONV(title));
#endif // wxUSE_LIBHILDON/!wxUSE_LIBHILDON

    g_object_ref(m_widget);

    if ( parentGTK )
    {
        gtk_window_set_transient_for(GTK_WINDOW(m_widget), parentGTK);
    }

#if !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    GtkColorSelection* sel = GTK_COLOR_SELECTION(
        gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(m_widget)));
    gtk_color_selection_set_has_palette(sel, true);
#endif // !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2

    return true;
}

int wxColourDialog::ShowModal()
{
    ColourDataToDialog();

    gint result = gtk_dialog_run(GTK_DIALOG(m_widget));
    gtk_widget_hide(m_widget);

    switch (result)
    {
        default:
            wxFAIL_MSG(wxT("unexpected GtkColorSelectionDialog return code"));
            // fall through

        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CLOSE:
            return wxID_CANCEL;

        case GTK_RESPONSE_OK:
            DialogToColourData();
            return wxID_OK;
    }
}

void wxColourDialog::ColourDataToDialog()
{
    const GdkColor * const
        col = m_data.GetColour().IsOk() ? m_data.GetColour().GetColor()
                                      : NULL;

#if wxUSE_LIBHILDON
    HildonColorSelector * const sel = HILDON_COLOR_SELECTOR(m_widget);
    hildon_color_selector_set_color(sel, const_cast<GdkColor *>(col));
#elif wxUSE_LIBHILDON2
    GdkColor clr;
    if (col)
        clr = *col;
    else {
        clr.pixel = 0;
        clr.red = 32768;
        clr.green = 32768;
        clr.blue = 32768;
    }

    hildon_color_chooser_dialog_set_color((HildonColorChooserDialog *)m_widget, &clr);
#else // !wxUSE_LIBHILDON2/!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    GtkColorSelection* sel = GTK_COLOR_SELECTION(
        gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(m_widget)));

    if ( col )
        gtk_color_selection_set_current_color(sel, col);

    // setup the palette:

    GdkColor colors[16];
    gint n_colors = 0;
    for (unsigned i = 0; i < 16; i++)
    {
        wxColour c = m_data.GetCustomColour(i);
        if (c.IsOk())
        {
            colors[n_colors] = *c.GetColor();
            n_colors++;
        }
    }

    wxGtkString pal(gtk_color_selection_palette_to_string(colors, n_colors));

    GtkSettings *settings = gtk_widget_get_settings(GTK_WIDGET(sel));
    g_object_set(settings, "gtk-color-palette", pal.c_str(), NULL);
#endif // wxUSE_LIBHILDON / wxUSE_LIBHILDON2 /!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
}

void wxColourDialog::DialogToColourData()
{
#if wxUSE_LIBHILDON
    HildonColorSelector * const sel = HILDON_COLOR_SELECTOR(m_widget);
    const GdkColor * const clr = hildon_color_selector_get_color(sel);
    if ( clr )
        m_data.SetColour(*clr);
#elif wxUSE_LIBHILDON2 // !wxUSE_LIBHILDON
    const GdkColor * const
    col = m_data.GetColour().IsOk() ? m_data.GetColour().GetColor() : NULL;

    GdkColor clr;
    if (col)
        clr = *col;
    else {
        clr.pixel = 0;
        clr.red = 32768;
        clr.green = 32768;
        clr.blue = 32768;
    }
    GdkColor new_color = clr;
    hildon_color_chooser_dialog_get_color((HildonColorChooserDialog *)m_widget, &new_color);

    m_data.SetColour(new_color);
#else // !wxUSE_LIBHILDON2

    GtkColorSelection* sel = GTK_COLOR_SELECTION(
        gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(m_widget)));

    GdkColor clr;
    gtk_color_selection_get_current_color(sel, &clr);
    m_data.SetColour(clr);

    // Extract custom palette:

    GtkSettings *settings = gtk_widget_get_settings(GTK_WIDGET(sel));
    gchar *pal;
    g_object_get(settings, "gtk-color-palette", &pal, NULL);

    GdkColor *colors;
    gint n_colors;
    if (gtk_color_selection_palette_from_string(pal, &colors, &n_colors))
    {
        for (int i = 0; i < wxMin(n_colors, 16); i++)
        {
            m_data.SetCustomColour(i, wxColour(colors[i]));
        }
        g_free(colors);
    }

    g_free(pal);
#endif // wxUSE_LIBHILDON / wxUSE_LIBHILDON2 /!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
}

#endif // wxUSE_COLOURDLG

