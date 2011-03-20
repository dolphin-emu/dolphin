/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/control.cpp
// Purpose:     wxControl implementation for wxGTK
// Author:      Robert Roebling
// Id:          $Id: control.cpp 67062 2011-02-27 12:48:07Z VZ $
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
#include "wx/gtk/private.h"
#include "wx/utils.h"
#include "wx/sysopt.h"

#include "wx/gtk/private/mnemonics.h"

// ============================================================================
// wxControl implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxControl creation
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxControl, wxWindow)

wxControl::wxControl()
{
}

bool wxControl::Create( wxWindow *parent,
                      wxWindowID id,
                      const wxPoint &pos,
                      const wxSize &size,
                      long style,
                      const wxValidator& validator,
                      const wxString &name )
{
    bool ret = wxWindow::Create(parent, id, pos, size, style, name);

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif

    return ret;
}

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
        GtkRequisition req;
        GTK_WIDGET_GET_CLASS(m_widget)->size_request(m_widget, &req);
        best.Set(req.width, req.height);
    }
    CacheBestSize(best);
    return best;
}

void wxControl::PostCreation(const wxSize& size)
{
    wxWindow::PostCreation();

    // NB: GetBestSize needs to know the style, otherwise it will assume
    //     default font and if the user uses a different font, determined
    //     best size will be different (typically, smaller) than the desired
    //     size. This call ensure that a style is available at the time
    //     GetBestSize is called.
    gtk_widget_ensure_style(m_widget);

    GTKApplyWidgetStyle();
    SetInitialSize(size);
}

// ----------------------------------------------------------------------------
// Work around a GTK+ bug whereby button is insensitive after being
// enabled
// ----------------------------------------------------------------------------

// Fix sensitivity due to bug in GTK+ < 2.14
void wxControl::GTKFixSensitivity(bool onlyIfUnderMouse)
{
    if (gtk_check_version(2,14,0)
#if wxUSE_SYSTEM_OPTIONS
        && (wxSystemOptions::GetOptionInt(wxT("gtk.control.disable-sensitivity-fix")) != 1)
#endif
        )
    {
        wxPoint pt = wxGetMousePosition();
        wxRect rect(ClientToScreen(wxPoint(0, 0)), GetSize());
        if (!onlyIfUnderMouse || rect.Contains(pt))
        {
            Hide();
            Show();
        }
    }
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
    gtk_widget_modify_style(GTK_WIDGET(w), style);
    gtk_widget_modify_style(gtk_frame_get_label_widget (w), style);
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
                                             bool useBase,
                                             int state)
{
    GtkStyle* style;
    wxVisualAttributes attr;

    style = gtk_rc_get_style(widget);
    if (!style)
        style = gtk_widget_get_default_style();

    if (!style)
    {
        return wxWindow::GetClassDefaultAttributes(wxWINDOW_VARIANT_NORMAL);
    }

    if (state == -1)
        state = GTK_STATE_NORMAL;

    // get the style's colours
    attr.colFg = wxColour(style->fg[state]);
    if (useBase)
        attr.colBg = wxColour(style->base[state]);
    else
        attr.colBg = wxColour(style->bg[state]);

    // get the style's font
    if ( !style->font_desc )
        style = gtk_widget_get_default_style();
    if ( style && style->font_desc )
    {
        wxNativeFontInfo info;
        info.description = pango_font_description_copy(style->font_desc);
        attr.font = wxFont(info);
    }
    else
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
            attr.font = wxFont(wxString::FromAscii(font_name));
        g_free (font_name);
    }

    return attr;
}


//static
wxVisualAttributes
wxControl::GetDefaultAttributesFromGTKWidget(wxGtkWidgetNew_t widget_new,
                                             bool useBase,
                                             int state)
{
    wxVisualAttributes attr;
    // NB: we need toplevel window so that GTK+ can find the right style
    GtkWidget *wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget* widget = widget_new();
    gtk_container_add(GTK_CONTAINER(wnd), widget);
    attr = GetDefaultAttributesFromGTKWidget(widget, useBase, state);
    gtk_widget_destroy(wnd);
    return attr;
}

//static
wxVisualAttributes
wxControl::GetDefaultAttributesFromGTKWidget(wxGtkWidgetNewFromStr_t widget_new,
                                             bool useBase,
                                             int state)
{
    wxVisualAttributes attr;
    // NB: we need toplevel window so that GTK+ can find the right style
    GtkWidget *wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget* widget = widget_new("");
    gtk_container_add(GTK_CONTAINER(wnd), widget);
    attr = GetDefaultAttributesFromGTKWidget(widget, useBase, state);
    gtk_widget_destroy(wnd);
    return attr;
}


//static
wxVisualAttributes
wxControl::GetDefaultAttributesFromGTKWidget(wxGtkWidgetNewFromAdj_t widget_new,
                                             bool useBase,
                                             int state)
{
    wxVisualAttributes attr;
    // NB: we need toplevel window so that GTK+ can find the right style
    GtkWidget *wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget* widget = widget_new(NULL);
    gtk_container_add(GTK_CONTAINER(wnd), widget);
    attr = GetDefaultAttributesFromGTKWidget(widget, useBase, state);
    gtk_widget_destroy(wnd);
    return attr;
}

#endif // wxUSE_CONTROLS
