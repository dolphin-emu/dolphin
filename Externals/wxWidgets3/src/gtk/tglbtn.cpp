/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/tglbtn.cpp
// Purpose:     Definition of the wxToggleButton class, which implements a
//              toggle button under wxGTK.
// Author:      John Norris, minor changes by Axel Schlueter
// Modified by:
// Created:     08.02.01
// RCS-ID:      $Id: tglbtn.cpp 64940 2010-07-13 13:29:13Z VZ $
// Copyright:   (c) 2000 Johnny C. Norris II
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_TOGGLEBTN

#include "wx/tglbtn.h"

#ifndef WX_PRECOMP
    #include "wx/button.h"
#endif

#include "wx/gtk/private.h"

extern bool      g_blockEventsOnDrag;

extern "C" {
static void gtk_togglebutton_clicked_callback(GtkWidget *WXUNUSED(widget), wxToggleButton *cb)
{
    if (!cb->m_hasVMT || g_blockEventsOnDrag)
        return;

    // Generate a wx event.
    wxCommandEvent event(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, cb->GetId());
    event.SetInt(cb->GetValue());
    event.SetEventObject(cb);
    cb->HandleWindowEvent(event);
}
}

wxDEFINE_EVENT( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEvent );

// ------------------------------------------------------------------------
// wxBitmapToggleButton
// ------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxBitmapToggleButton, wxControl)

bool wxBitmapToggleButton::Create(wxWindow *parent, wxWindowID id,
                            const wxBitmap &label, const wxPoint &pos,
                            const wxSize &size, long style,
                            const wxValidator& validator,
                            const wxString &name)
{
    if (!PreCreation(parent, pos, size) ||
       !CreateBase(parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG(wxT("wxBitmapToggleButton creation failed"));
        return false;
    }

    // Create the gtk widget.
    m_widget = gtk_toggle_button_new();
    g_object_ref(m_widget);

    if (style & wxNO_BORDER)
        gtk_button_set_relief( GTK_BUTTON(m_widget), GTK_RELIEF_NONE );

    m_bitmap = label;
    OnSetBitmap();

    g_signal_connect (m_widget, "clicked",
                      G_CALLBACK (gtk_togglebutton_clicked_callback),
                      this);

    m_parent->DoAddChild(this);

    PostCreation(size);

    return true;
}

void wxBitmapToggleButton::GTKDisableEvents()
{
    g_signal_handlers_block_by_func(m_widget,
                                (gpointer) gtk_togglebutton_clicked_callback, this);
}

void wxBitmapToggleButton::GTKEnableEvents()
{
    g_signal_handlers_unblock_by_func(m_widget,
                                (gpointer) gtk_togglebutton_clicked_callback, this);
}

// void SetValue(bool state)
// Set the value of the toggle button.
void wxBitmapToggleButton::SetValue(bool state)
{
    wxCHECK_RET(m_widget != NULL, wxT("invalid toggle button"));

    if (state == GetValue())
        return;

    GTKDisableEvents();

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_widget), state);

    GTKEnableEvents();
}

// bool GetValue() const
// Get the value of the toggle button.
bool wxBitmapToggleButton::GetValue() const
{
    wxCHECK_MSG(m_widget != NULL, false, wxT("invalid toggle button"));

    return gtk_toggle_button_get_active((GtkToggleButton*)m_widget);
}

void wxBitmapToggleButton::SetLabel(const wxBitmap& label)
{
    wxCHECK_RET(m_widget != NULL, wxT("invalid toggle button"));

    m_bitmap = label;
    InvalidateBestSize();

    OnSetBitmap();
}

void wxBitmapToggleButton::OnSetBitmap()
{
    if (!m_bitmap.Ok()) return;

    GtkWidget* image = ((GtkBin*)m_widget)->child;
    if (image == NULL)
    {
        image = gtk_image_new();
        gtk_widget_show(image);
        gtk_container_add((GtkContainer*)m_widget, image);
    }
    // always use pixbuf, because pixmap mask does not
    // work with disabled images in some themes
    gtk_image_set_from_pixbuf((GtkImage*)image, m_bitmap.GetPixbuf());
}

bool wxBitmapToggleButton::Enable(bool enable /*=true*/)
{
    bool isEnabled = IsEnabled();

    if (!wxControl::Enable(enable))
        return false;

    gtk_widget_set_sensitive(GTK_BIN(m_widget)->child, enable);

    if (!isEnabled && enable)
    {
        GTKFixSensitivity();
    }

    return true;
}

void wxBitmapToggleButton::DoApplyWidgetStyle(GtkRcStyle *style)
{
    gtk_widget_modify_style(m_widget, style);
    gtk_widget_modify_style(GTK_BIN(m_widget)->child, style);
}

GdkWindow *
wxBitmapToggleButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_BUTTON(m_widget)->event_window;
}

// Get the "best" size for this control.
wxSize wxBitmapToggleButton::DoGetBestSize() const
{
    wxSize best;

    if (m_bitmap.Ok())
    {
        int border = HasFlag(wxNO_BORDER) ? 4 : 10;
        best.x = m_bitmap.GetWidth()+border;
        best.y = m_bitmap.GetHeight()+border;
    }
    CacheBestSize(best);
    return best;
}


// static
wxVisualAttributes
wxBitmapToggleButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_toggle_button_new);
}


// ------------------------------------------------------------------------
// wxToggleButton
// ------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxToggleButton, wxControl)

bool wxToggleButton::Create(wxWindow *parent, wxWindowID id,
                            const wxString &label, const wxPoint &pos,
                            const wxSize &size, long style,
                            const wxValidator& validator,
                            const wxString &name)
{
    if (!PreCreation(parent, pos, size) ||
        !CreateBase(parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG(wxT("wxToggleButton creation failed"));
        return false;
    }

    // Create the gtk widget.
    m_widget = gtk_toggle_button_new_with_mnemonic("");
    g_object_ref(m_widget);

    SetLabel(label);

    g_signal_connect (m_widget, "clicked",
                      G_CALLBACK (gtk_togglebutton_clicked_callback),
                      this);

    m_parent->DoAddChild(this);

    PostCreation(size);

    return true;
}

void wxToggleButton::GTKDisableEvents()
{
    g_signal_handlers_block_by_func(m_widget,
                                (gpointer) gtk_togglebutton_clicked_callback, this);
}

void wxToggleButton::GTKEnableEvents()
{
    g_signal_handlers_unblock_by_func(m_widget,
                                (gpointer) gtk_togglebutton_clicked_callback, this);
}

// void SetValue(bool state)
// Set the value of the toggle button.
void wxToggleButton::SetValue(bool state)
{
    wxCHECK_RET(m_widget != NULL, wxT("invalid toggle button"));

    if (state == GetValue())
        return;

    GTKDisableEvents();

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_widget), state);

    GTKEnableEvents();
}

// bool GetValue() const
// Get the value of the toggle button.
bool wxToggleButton::GetValue() const
{
    wxCHECK_MSG(m_widget != NULL, false, wxT("invalid toggle button"));

    return GTK_TOGGLE_BUTTON(m_widget)->active;
}

void wxToggleButton::SetLabel(const wxString& label)
{
    wxCHECK_RET(m_widget != NULL, wxT("invalid toggle button"));

    wxControl::SetLabel(label);

    const wxString labelGTK = GTKConvertMnemonics(label);

    gtk_button_set_label(GTK_BUTTON(m_widget), wxGTK_CONV(labelGTK));

    GTKApplyWidgetStyle( false );
}

bool wxToggleButton::Enable(bool enable /*=true*/)
{
    if (!base_type::Enable(enable))
        return false;

    gtk_widget_set_sensitive(GTK_BIN(m_widget)->child, enable);

    if (enable)
        GTKFixSensitivity();

    return true;
}

void wxToggleButton::DoApplyWidgetStyle(GtkRcStyle *style)
{
    gtk_widget_modify_style(m_widget, style);
    gtk_widget_modify_style(GTK_BIN(m_widget)->child, style);
}

GdkWindow *
wxToggleButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_BUTTON(m_widget)->event_window;
}

// Get the "best" size for this control.
wxSize wxToggleButton::DoGetBestSize() const
{
    wxSize ret(wxControl::DoGetBestSize());

    if (!HasFlag(wxBU_EXACTFIT))
    {
        if (ret.x < 80) ret.x = 80;
    }

    CacheBestSize(ret);
    return ret;
}

// static
wxVisualAttributes
wxToggleButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_toggle_button_new);
}

#endif // wxUSE_TOGGLEBTN
