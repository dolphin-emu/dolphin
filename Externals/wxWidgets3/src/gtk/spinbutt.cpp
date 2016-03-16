/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/spinbutt.cpp
// Purpose:     wxSpinButton
// Author:      Robert
// Modified by:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SPINBTN

#include "wx/spinbutt.h"

#include <gtk/gtk.h>

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool   g_blockEventsOnDrag;

//-----------------------------------------------------------------------------
// "value_changed"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_value_changed(GtkSpinButton* spinbutton, wxSpinButton* win)
{
    const double value = gtk_spin_button_get_value(spinbutton);
    const int pos = int(value);
    const int oldPos = win->m_pos;
    if (g_blockEventsOnDrag || pos == oldPos)
    {
        win->m_pos = pos;
        return;
    }

    wxSpinEvent event(pos > oldPos ? wxEVT_SCROLL_LINEUP : wxEVT_SCROLL_LINEDOWN, win->GetId());
    event.SetPosition(pos);
    event.SetEventObject(win);

    if ((win->HandleWindowEvent( event )) &&
        !event.IsAllowed() )
    {
        /* program has vetoed */
        // this will cause another "value_changed" signal,
        // but because pos == oldPos nothing will happen
        gtk_spin_button_set_value(spinbutton, oldPos);
        return;
    }

    win->m_pos = pos;

    /* always send a thumbtrack event */
    wxSpinEvent event2(wxEVT_SCROLL_THUMBTRACK, win->GetId());
    event2.SetPosition(pos);
    event2.SetEventObject(win);
    win->HandleWindowEvent(event2);
}
}

//-----------------------------------------------------------------------------
// wxSpinButton
//-----------------------------------------------------------------------------

wxSpinButton::wxSpinButton()
{
    m_pos = 0;
}

bool wxSpinButton::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxString& name)
{
    if (!PreCreation(parent, pos, size) ||
        !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name))
    {
        wxFAIL_MSG( wxT("wxSpinButton creation failed") );
        return false;
    }

    m_pos = 0;

    m_widget = gtk_spin_button_new_with_range(0, 100, 1);
    g_object_ref(m_widget);

    gtk_entry_set_width_chars(GTK_ENTRY(m_widget), 0);
#if GTK_CHECK_VERSION(3,12,0)
    if (gtk_check_version(3,12,0) == NULL)
        gtk_entry_set_max_width_chars(GTK_ENTRY(m_widget), 0);
#endif
    gtk_spin_button_set_wrap( GTK_SPIN_BUTTON(m_widget),
                              (int)(m_windowStyle & wxSP_WRAP) );

    g_signal_connect_after(
        m_widget, "value_changed", G_CALLBACK(gtk_value_changed), this);

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

int wxSpinButton::GetMin() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    double min;
    gtk_spin_button_get_range((GtkSpinButton*)m_widget, &min, NULL);
    return int(min);
}

int wxSpinButton::GetMax() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    double max;
    gtk_spin_button_get_range((GtkSpinButton*)m_widget, NULL, &max);
    return int(max);
}

int wxSpinButton::GetValue() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    return m_pos;
}

void wxSpinButton::SetValue( int value )
{
    wxCHECK_RET( (m_widget != NULL), wxT("invalid spin button") );

    GtkDisableEvents();
    gtk_spin_button_set_value((GtkSpinButton*)m_widget, value);
    m_pos = int(gtk_spin_button_get_value((GtkSpinButton*)m_widget));
    GtkEnableEvents();
}

void wxSpinButton::SetRange(int minVal, int maxVal)
{
    wxCHECK_RET( (m_widget != NULL), wxT("invalid spin button") );

    GtkDisableEvents();
    gtk_spin_button_set_range((GtkSpinButton*)m_widget, minVal, maxVal);
    m_pos = int(gtk_spin_button_get_value((GtkSpinButton*)m_widget));
    GtkEnableEvents();
}

bool wxSpinButton::Enable( bool enable )
{
    if (!base_type::Enable(enable))
        return false;

    // Work around lack of visual update when enabling
    if (enable)
        GTKFixSensitivity(false /* fix even if not under mouse */);

    return true;
}

void wxSpinButton::GtkDisableEvents() const
{
    g_signal_handlers_block_by_func(m_widget,
        (gpointer)gtk_value_changed, (void*) this);
}

void wxSpinButton::GtkEnableEvents() const
{
    g_signal_handlers_unblock_by_func(m_widget,
        (gpointer)gtk_value_changed, (void*) this);
}

GdkWindow *wxSpinButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED_IN_GTK2(windows)) const
{
#ifdef __WXGTK3__
    GTKFindWindow(m_widget, windows);
    return NULL;
#else
    return GTK_SPIN_BUTTON(m_widget)->panel;
#endif
}

wxSize wxSpinButton::DoGetBestSize() const
{
    wxSize best = base_type::DoGetBestSize();
#ifdef __WXGTK3__
    GtkStyleContext* sc = gtk_widget_get_style_context(m_widget);
    GtkBorder pad = { 0, 0, 0, 0 };
    gtk_style_context_get_padding(sc, gtk_style_context_get_state(sc), &pad);
    best.x -= pad.left + pad.right;
#else
    gtk_widget_ensure_style(m_widget);
    int w = PANGO_PIXELS(pango_font_description_get_size(m_widget->style->font_desc));
    w &= ~1;
    if (w < 6)
        w = 6;
    best.x = w + 2 * m_widget->style->xthickness;
#endif
    CacheBestSize(best);
    return best;
}

// static
wxVisualAttributes
wxSpinButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_spin_button_new_with_range(0, 100, 1));
}

#endif // wxUSE_SPINBTN
