/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/colordlg.cpp
// Purpose:     Native wxColourDialog for GTK+
// Author:      Vaclav Slavik
// Modified by:
// Created:     2004/06/04
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

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"
#include "wx/gtk/private/dialogcount.h"

extern "C" {
static void response(GtkDialog*, int response_id, wxColourDialog* win)
{
    win->EndModal(response_id == GTK_RESPONSE_OK ? wxID_OK : wxID_CANCEL);
}
}

wxIMPLEMENT_DYNAMIC_CLASS(wxColourDialog, wxDialog);

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

    wxString title(_("Choose colour"));
    m_widget = gtk_color_selection_dialog_new(wxGTK_CONV(title));

    g_object_ref(m_widget);

    if ( parentGTK )
    {
        gtk_window_set_transient_for(GTK_WINDOW(m_widget), parentGTK);
    }

    GtkColorSelection* sel = GTK_COLOR_SELECTION(
        gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(m_widget)));
    gtk_color_selection_set_has_palette(sel, true);
    gtk_color_selection_set_has_opacity_control(sel, m_data.GetChooseAlpha());

    return true;
}

int wxColourDialog::ShowModal()
{
    ColourDataToDialog();

    gulong id = g_signal_connect(m_widget, "response", G_CALLBACK(response), this);
    int rc = wxDialog::ShowModal();
    g_signal_handler_disconnect(m_widget, id);

    if (rc == wxID_OK)
        DialogToColourData();

    return rc;
}

void wxColourDialog::ColourDataToDialog()
{
    GtkColorSelection* sel = GTK_COLOR_SELECTION(
        gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(m_widget)));

    const wxColour& color = m_data.GetColour();
    if (color.IsOk())
    {
#ifdef __WXGTK3__
        gtk_color_selection_set_current_rgba(sel, color);
#else
        gtk_color_selection_set_current_color(sel, color.GetColor());
        // Convert alpha range: [0,255] -> [0,65535]
        gtk_color_selection_set_current_alpha(sel, 257*color.Alpha());
#endif
    }

    // setup the palette:

    GdkColor colors[wxColourData::NUM_CUSTOM];
    gint n_colors = 0;
    for (unsigned i = 0; i < WXSIZEOF(colors); i++)
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
}

void wxColourDialog::DialogToColourData()
{
    GtkColorSelection* sel = GTK_COLOR_SELECTION(
        gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(m_widget)));

#ifdef __WXGTK3__
    GdkRGBA clr;
    gtk_color_selection_get_current_rgba(sel, &clr);
    m_data.SetColour(clr);
#else
    GdkColor clr;
    gtk_color_selection_get_current_color(sel, &clr);
    // Set RGB colour
    wxColour cRGB(clr);
    guint16 alpha = gtk_color_selection_get_current_alpha(sel);
    // Set RGBA colour (convert alpha range: [0,65535] -> [0,255]).
    wxColour cRGBA(cRGB.Red(), cRGB.Green(), cRGB.Blue(), alpha/257);
    m_data.SetColour(cRGBA);
#endif

    // Extract custom palette:

    GtkSettings *settings = gtk_widget_get_settings(GTK_WIDGET(sel));
    gchar *pal;
    g_object_get(settings, "gtk-color-palette", &pal, NULL);

    GdkColor *colors;
    gint n_colors;
    if (gtk_color_selection_palette_from_string(pal, &colors, &n_colors))
    {
        for (int i = 0; i < n_colors && i < wxColourData::NUM_CUSTOM; i++)
        {
            m_data.SetCustomColour(i, wxColour(colors[i]));
        }
        g_free(colors);
    }

    g_free(pal);
}

#endif // wxUSE_COLOURDLG

