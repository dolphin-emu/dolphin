/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/control.cpp
// Purpose:     wxControl implementation for wxGTK
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart and Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_CONTROLS

#include "wx/control.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/fontutil.h"
#include "wx/utils.h"
#include "wx/sysopt.h"

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/mnemonics.h"

// ============================================================================
// wxControl implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxControl creation
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxControl, wxWindow);

wxControl::wxControl()
{
}

bool wxControl::Create( wxWindow *parent,
                      wxWindowID id,
                      const wxPoint &pos,
                      const wxSize &size,
                      long style,
                      const wxValidator& wxVALIDATOR_PARAM(validator),
                      const wxString &name )
{
    bool ret = wxWindow::Create(parent, id, pos, size, style, name);

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif

    return ret;
}

#ifdef __WXGTK3__
bool wxControl::SetFont(const wxFont& font)
{
    const bool changed = base_type::SetFont(font);
    if (changed && !gtk_widget_get_realized(m_widget) && gtk_check_version(3,5,0))
    {
        // GTK defers sending "style-updated" until widget is realized, but
        // GetBestSize() won't compute correct result until the signal is sent,
        // so we have to do it now
        // But don't bother for GTK > 3.4, the change won't take effect until
        // GTK updates it's style cache
        g_signal_emit_by_name(m_widget, "style-updated");
    }
    return changed;
}
#endif

wxSize wxControl::DoGetBestSize() const
{
    // Do not return any arbitrary default value...
    wxASSERT_MSG( m_widget, wxT("DoGetBestSize called before creation") );

    wxSize best;
    if (m_wxwindow)
    {
        // this is not a native control, size_request is likely to be (0,0)
        best = wxControlBase::DoGetBestSize();
    }
    else
    {
        best = GTKGetPreferredSize(m_widget);
    }

    return best;
}

void wxControl::PostCreation(const wxSize& size)
{
    wxWindow::PostCreation();

#ifndef __WXGTK3__
    // NB: GetBestSize needs to know the style, otherwise it will assume
    //     default font and if the user uses a different font, determined
    //     best size will be different (typically, smaller) than the desired
    //     size. This call ensure that a style is available at the time
    //     GetBestSize is called.
    gtk_widget_ensure_style(m_widget);
#endif

    SetInitialSize(size);
}

// ----------------------------------------------------------------------------
// Work around a GTK+ bug whereby button is insensitive after being
// enabled
// ----------------------------------------------------------------------------

// Fix sensitivity due to bug in GTK+ < 2.14
void wxControl::GTKFixSensitivity(bool WXUNUSED_IN_GTK3(onlyIfUnderMouse))
{
#ifndef __WXGTK3__
    if (gtk_check_version(2,14,0)
#if wxUSE_SYSTEM_OPTIONS
        && (wxSystemOptions::GetOptionInt(wxT("gtk.control.disable-sensitivity-fix")) != 1)
#endif
        )
    {
        if (!onlyIfUnderMouse || GetScreenRect().Contains(wxGetMousePosition()))
        {
            Hide();
            Show();
        }
    }
#endif
}

// ----------------------------------------------------------------------------
// wxControl dealing with labels
// ----------------------------------------------------------------------------

void wxControl::GTKSetLabelForLabel(GtkLabel *w, const wxString& label)
{
    const wxString labelGTK = GTKConvertMnemonics(label);
    gtk_label_set_text_with_mnemonic(w, wxGTK_CONV(labelGTK));
}

#if wxUSE_MARKUP

void wxControl::GTKSetLabelWithMarkupForLabel(GtkLabel *w, const wxString& label)
{
    const wxString labelGTK = GTKConvertMnemonicsWithMarkup(label);
    gtk_label_set_markup_with_mnemonic(w, wxGTK_CONV(labelGTK));
}

#endif // wxUSE_MARKUP

// ----------------------------------------------------------------------------
// GtkFrame helpers
//
// GtkFrames do in fact support mnemonics in GTK2+ but not through
// gtk_frame_set_label, rather you need to use a custom label widget
// instead (idea gleaned from the native gtk font dialog code in GTK)
// ----------------------------------------------------------------------------

GtkWidget* wxControl::GTKCreateFrame(const wxString& label)
{
    const wxString labelGTK = GTKConvertMnemonics(label);
    GtkWidget* labelwidget = gtk_label_new_with_mnemonic(wxGTK_CONV(labelGTK));
    gtk_widget_show(labelwidget); // without this it won't show...

    GtkWidget* framewidget = gtk_frame_new(NULL);
    gtk_frame_set_label_widget(GTK_FRAME(framewidget), labelwidget);

    return framewidget; // note that the label is already set so you'll
                        // only need to call wxControl::SetLabel afterwards
}

void wxControl::GTKSetLabelForFrame(GtkFrame *w, const wxString& label)
{
    wxControlBase::SetLabel(label);

    GtkLabel* labelwidget = GTK_LABEL(gtk_frame_get_label_widget(w));
    GTKSetLabelForLabel(labelwidget, label);
}

void wxControl::GTKFrameApplyWidgetStyle(GtkFrame* w, GtkRcStyle* style)
{
    GTKApplyStyle(GTK_WIDGET(w), style);
    GTKApplyStyle(gtk_frame_get_label_widget(w), style);
}

void wxControl::GTKFrameSetMnemonicWidget(GtkFrame* w, GtkWidget* widget)
{
    GtkLabel* labelwidget = GTK_LABEL(gtk_frame_get_label_widget(w));

    gtk_label_set_mnemonic_widget(labelwidget, widget);
}

// ----------------------------------------------------------------------------
// worker function implementing GTK*Mnemonics() functions
// ----------------------------------------------------------------------------

/* static */
wxString wxControl::GTKRemoveMnemonics(const wxString& label)
{
    return wxGTKRemoveMnemonics(label);
}

/* static */
wxString wxControl::GTKConvertMnemonics(const wxString& label)
{
    return wxConvertMnemonicsToGTK(label);
}

/* static */
wxString wxControl::GTKConvertMnemonicsWithMarkup(const wxString& label)
{
    return wxConvertMnemonicsToGTKMarkup(label);
}

// ----------------------------------------------------------------------------
// wxControl styles (a.k.a. attributes)
// ----------------------------------------------------------------------------

wxVisualAttributes wxControl::GetDefaultAttributes() const
{
    return GetDefaultAttributesFromGTKWidget(m_widget,
                                             UseGTKStyleBase());
}

// static
wxVisualAttributes
wxControl::GetDefaultAttributesFromGTKWidget(GtkWidget* widget,
                                             bool WXUNUSED_IN_GTK3(useBase),
                                             int state)
{
    wxVisualAttributes attr;

    GtkWidget* tlw = NULL;
    if (gtk_widget_get_parent(widget) == NULL)
    {
        tlw = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_container_add(GTK_CONTAINER(tlw), widget);
    }

#ifdef __WXGTK3__
    GtkStateFlags stateFlag = GTK_STATE_FLAG_NORMAL;
    if (state)
    {
        wxASSERT(state == GTK_STATE_ACTIVE);
        stateFlag = GTK_STATE_FLAG_ACTIVE;
    }
    GtkStyleContext* sc = gtk_widget_get_style_context(widget);
    GdkRGBA *fc, *bc;
    wxNativeFontInfo info;
    gtk_style_context_set_state(sc, stateFlag);
    gtk_style_context_get(sc, stateFlag,
        "color", &fc, "background-color", &bc,
        GTK_STYLE_PROPERTY_FONT, &info.description, NULL);
    attr.colFg = wxColour(*fc);
    attr.colBg = wxColour(*bc);
    attr.font = wxFont(info);
    gdk_rgba_free(fc);
    gdk_rgba_free(bc);
#else
    GtkStyle* style;

    style = gtk_rc_get_style(widget);
    if (!style)
        style = gtk_widget_get_default_style();

    if (style)
    {
        // get the style's colours
        attr.colFg = wxColour(style->fg[state]);
        if (useBase)
            attr.colBg = wxColour(style->base[state]);
        else
            attr.colBg = wxColour(style->bg[state]);

        // get the style's font
        if (!style->font_desc)
            style = gtk_widget_get_default_style();
        if (style && style->font_desc)
        {
            wxNativeFontInfo info;
            info.description = style->font_desc;
            attr.font = wxFont(info);
            info.description = NULL;
        }
    }
    else
        attr = wxWindow::GetClassDefaultAttributes(wxWINDOW_VARIANT_NORMAL);
#endif

    if (!attr.font.IsOk())
    {
        GtkSettings *settings = gtk_settings_get_default();
        gchar *font_name = NULL;
        g_object_get ( settings,
                       "gtk-font-name",
                       &font_name,
                       NULL);
        if (!font_name)
            attr.font = wxSystemSettings::GetFont( wxSYS_DEFAULT_GUI_FONT );
        else
        {
            attr.font = wxFont(wxString::FromUTF8(font_name));
            g_free(font_name);
        }
    }

    if (tlw)
        gtk_widget_destroy(tlw);

    return attr;
}

// This is not the same as GetBestSize() because that size may have
// been recalculated and cached by us. We want GTK+ information.
wxSize wxControl::GTKGetPreferredSize(GtkWidget* widget) const
{
    GtkRequisition req;
#ifdef __WXGTK3__
    int w, h;
    gtk_widget_get_size_request(widget, &w, &h);
    gtk_widget_set_size_request(widget, -1, -1);
    gtk_widget_get_preferred_size(widget, NULL, &req);
    gtk_widget_set_size_request(widget, w, h);
#else
    GTK_WIDGET_GET_CLASS(widget)->size_request(widget, &req);
#endif

    return wxSize(req.width, req.height);
}

wxPoint wxControl::GTKGetEntryMargins(GtkEntry* entry) const
{
    wxPoint marg(0, 0);

#ifndef __WXGTK3__
#if GTK_CHECK_VERSION(2,10,0)
    // The margins we have previously set
    const GtkBorder* border = NULL;
    if (gtk_check_version(2,10,0) == NULL)
        border = gtk_entry_get_inner_border(entry);

    if ( border )
    {
        marg.x = border->left + border->right;
        marg.y = border->top + border->bottom;
    }
#endif // GTK+ 2.10+
#else // GTK+ 3
    // Gtk3 does not use inner border, but StyleContext and CSS
    // TODO: implement it, starting with wxTextEntry::DoSetMargins()
#endif // GTK+ 2/3

    int x, y;
    gtk_entry_get_layout_offsets(entry, &x, &y);
    // inner borders are included. Substract them so we can get other margins
    x -= marg.x;
    y -= marg.y;
    marg.x += 2 * x + 2;
    marg.y += 2 * y + 2;

    return marg;
}


#endif // wxUSE_CONTROLS
