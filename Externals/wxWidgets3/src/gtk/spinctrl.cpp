/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/spinctrl.cpp
// Purpose:     wxSpinCtrl
// Author:      Robert
// Modified by:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SPINCTRL

#include "wx/spinctrl.h"

#ifndef WX_PRECOMP
    #include "wx/textctrl.h"    // for wxEVT_TEXT
    #include "wx/utils.h"
    #include "wx/wxcrtvararg.h"
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool   g_blockEventsOnDrag;

//-----------------------------------------------------------------------------
// "value_changed"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_value_changed(GtkSpinButton* spinbutton, wxSpinCtrlGTKBase* win)
{
    if (g_blockEventsOnDrag)
        return;

    if (wxIsKindOf(win, wxSpinCtrl))
    {
        wxSpinEvent event(wxEVT_SPINCTRL, win->GetId());
        event.SetEventObject( win );
        event.SetPosition(static_cast<wxSpinCtrl*>(win)->GetValue());
        event.SetString(gtk_entry_get_text(GTK_ENTRY(spinbutton)));
        win->HandleWindowEvent( event );
    }
    else // wxIsKindOf(win, wxSpinCtrlDouble)
    {
        wxSpinDoubleEvent event( wxEVT_SPINCTRLDOUBLE, win->GetId());
        event.SetEventObject( win );
        event.SetValue(static_cast<wxSpinCtrlDouble*>(win)->GetValue());
        event.SetString(gtk_entry_get_text(GTK_ENTRY(spinbutton)));
        win->HandleWindowEvent( event );
    }
}
}

//-----------------------------------------------------------------------------
//  "changed"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_changed(GtkSpinButton* spinbutton, wxSpinCtrl* win)
{
    wxCommandEvent event( wxEVT_TEXT, win->GetId() );
    event.SetEventObject( win );
    event.SetString(gtk_entry_get_text(GTK_ENTRY(spinbutton)));
    event.SetInt(win->GetValue());
    win->HandleWindowEvent( event );
}
}

// ----------------------------------------------------------------------------
// wxSpinCtrlEventDisabler: helper to temporarily disable GTK+ events
// ----------------------------------------------------------------------------

class wxSpinCtrlEventDisabler
{
public:
    wxEXPLICIT wxSpinCtrlEventDisabler(wxSpinCtrlGTKBase* spin)
        : m_spin(spin)
    {
        m_spin->GtkDisableEvents();
    }

    ~wxSpinCtrlEventDisabler()
    {
        m_spin->GtkEnableEvents();
    }

private:
    wxSpinCtrlGTKBase* const m_spin;

    wxDECLARE_NO_COPY_CLASS(wxSpinCtrlEventDisabler);
};

//-----------------------------------------------------------------------------
// wxSpinCtrlGTKBase
//-----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxSpinCtrlGTKBase, wxSpinCtrlBase)
    EVT_CHAR(wxSpinCtrlGTKBase::OnChar)
wxEND_EVENT_TABLE()

bool wxSpinCtrlGTKBase::Create(wxWindow *parent, wxWindowID id,
                        const wxString& value,
                        const wxPoint& pos,  const wxSize& size,
                        long style,
                        double min, double max, double initial, double inc,
                        const wxString& name)
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
    {
        wxFAIL_MSG( wxT("wxSpinCtrlGTKBase creation failed") );
        return false;
    }

    m_widget = gtk_spin_button_new_with_range(min, max, inc);
    g_object_ref(m_widget);

    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m_widget), initial);

    gfloat align;
    if ( HasFlag(wxALIGN_RIGHT) )
        align = 1.0;
    else if ( HasFlag(wxALIGN_CENTRE) )
        align = 0.5;
    else
        align = 0.0;

    gtk_entry_set_alignment(GTK_ENTRY(m_widget), align);

    gtk_spin_button_set_wrap( GTK_SPIN_BUTTON(m_widget),
                              (int)(m_windowStyle & wxSP_WRAP) );

    g_signal_connect_after(m_widget, "value_changed", G_CALLBACK(gtk_value_changed), this);
    g_signal_connect_after(m_widget, "changed", G_CALLBACK(gtk_changed), this);

    m_parent->DoAddChild( this );

    PostCreation(size);

    if (!value.empty())
    {
        SetValue(value);
    }

    return true;
}

double wxSpinCtrlGTKBase::DoGetValue() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    // Get value directly from current control text, just as
    // gtk_spin_button_update() would do. Calling gtk_spin_button_update() causes
    // a redraw, which causes an idle event, so if GetValue() is called from
    // a UI update handler, you get a never ending sequence of idle events. It
    // also forces the text into valid range, which wxMSW GetValue() does not do.
    static unsigned sig_id;
    if (sig_id == 0)
        sig_id = g_signal_lookup("input", GTK_TYPE_SPIN_BUTTON);
    double value;
    int handled = 0;
    g_signal_emit(m_widget, sig_id, 0, &value, &handled);
    if (!handled)
        value = g_strtod(gtk_entry_get_text(GTK_ENTRY(m_widget)), NULL);
    GtkAdjustment* adj =
        gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(m_widget));
    const double lower = gtk_adjustment_get_lower(adj);
    const double upper = gtk_adjustment_get_upper(adj);
    if (value < lower)
        value = lower;
    else if (value > upper)
        value = upper;

    return value;
}

double wxSpinCtrlGTKBase::DoGetMin() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    double min = 0;
    gtk_spin_button_get_range( GTK_SPIN_BUTTON(m_widget), &min, NULL);
    return min;
}

double wxSpinCtrlGTKBase::DoGetMax() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    double max = 0;
    gtk_spin_button_get_range( GTK_SPIN_BUTTON(m_widget), NULL, &max);
    return max;
}

double wxSpinCtrlGTKBase::DoGetIncrement() const
{
    wxCHECK_MSG( (m_widget != NULL), 0, wxT("invalid spin button") );

    double inc = 0;
    gtk_spin_button_get_increments( GTK_SPIN_BUTTON(m_widget), &inc, NULL);
    return inc;
}

bool wxSpinCtrlGTKBase::GetSnapToTicks() const
{
    wxCHECK_MSG(m_widget, false, "invalid spin button");

    return gtk_spin_button_get_snap_to_ticks( GTK_SPIN_BUTTON(m_widget) ) != 0;
}

void wxSpinCtrlGTKBase::SetValue( const wxString& value )
{
    wxCHECK_RET( (m_widget != NULL), wxT("invalid spin button") );

    double n;
    if ( wxSscanf(value, "%lg", &n) == 1 )
    {
        // a number - set it, let DoSetValue round for int value
        DoSetValue(n);
        return;
    }

    // invalid number - set text as is (wxMSW compatible)
    wxSpinCtrlEventDisabler disable(this);
    gtk_entry_set_text( GTK_ENTRY(m_widget), wxGTK_CONV( value ) );
}

void wxSpinCtrlGTKBase::DoSetValue( double value )
{
    wxCHECK_RET( (m_widget != NULL), wxT("invalid spin button") );

    wxSpinCtrlEventDisabler disable(this);
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m_widget), value);
}

void wxSpinCtrlGTKBase::SetSnapToTicks(bool snap_to_ticks)
{
    wxCHECK_RET( (m_widget != NULL), "invalid spin button" );

    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(m_widget), snap_to_ticks);
}

void wxSpinCtrlGTKBase::SetSelection(long from, long to)
{
    // translate from wxWidgets conventions to GTK+ ones: (-1, -1) means the
    // entire range
    if ( from == -1 && to == -1 )
    {
        from = 0;
        to = INT_MAX;
    }

    gtk_editable_select_region( GTK_EDITABLE(m_widget), (gint)from, (gint)to );
}

void wxSpinCtrlGTKBase::DoSetRange(double minVal, double maxVal)
{
    wxCHECK_RET( (m_widget != NULL), wxT("invalid spin button") );

    wxSpinCtrlEventDisabler disable(this);
    gtk_spin_button_set_range( GTK_SPIN_BUTTON(m_widget), minVal, maxVal);
}

void wxSpinCtrlGTKBase::DoSetIncrement(double inc)
{
    wxCHECK_RET( m_widget, "invalid spin button" );

    wxSpinCtrlEventDisabler disable(this);

    // Preserve the old page value when changing just the increment.
    double page = 10*inc;
    gtk_spin_button_get_increments( GTK_SPIN_BUTTON(m_widget), NULL, &page);

    gtk_spin_button_set_increments( GTK_SPIN_BUTTON(m_widget), inc, page);
}

void wxSpinCtrlGTKBase::GtkDisableEvents() const
{
    g_signal_handlers_block_by_func( m_widget,
        (gpointer)gtk_value_changed, (void*) this);

    g_signal_handlers_block_by_func(m_widget,
        (gpointer)gtk_changed, (void*) this);
}

void wxSpinCtrlGTKBase::GtkEnableEvents() const
{
    g_signal_handlers_unblock_by_func(m_widget,
        (gpointer)gtk_value_changed, (void*) this);

    g_signal_handlers_unblock_by_func(m_widget,
        (gpointer)gtk_changed, (void*) this);
}

void wxSpinCtrlGTKBase::OnChar( wxKeyEvent &event )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid spin ctrl") );

    if (event.GetKeyCode() == WXK_RETURN)
    {
        wxWindow *top_frame = wxGetTopLevelParent(m_parent);

        if ( GTK_IS_WINDOW(top_frame->m_widget) )
        {
            GtkWindow *window = GTK_WINDOW(top_frame->m_widget);
            if ( window )
            {
                GtkWidget* widgetDef = gtk_window_get_default_widget(window);

                if ( widgetDef )
                {
                    gtk_widget_activate(widgetDef);
                    return;
                }
            }
        }
    }

    if ((event.GetKeyCode() == WXK_RETURN) && (m_windowStyle & wxTE_PROCESS_ENTER))
    {
        wxCommandEvent evt( wxEVT_TEXT_ENTER, m_windowId );
        evt.SetEventObject(this);
        GtkSpinButton *gsb = GTK_SPIN_BUTTON(m_widget);
        wxString val = wxGTK_CONV_BACK( gtk_entry_get_text( &gsb->entry ) );
        evt.SetString( val );
        if (HandleWindowEvent(evt)) return;
    }

    event.Skip();
}

GdkWindow *wxSpinCtrlGTKBase::GTKGetWindow(wxArrayGdkWindows& windows) const
{
#ifdef __WXGTK3__
    GTKFindWindow(m_widget, windows);
#else
    GtkSpinButton* spinbutton = GTK_SPIN_BUTTON(m_widget);

    windows.push_back(spinbutton->entry.text_area);
    windows.push_back(spinbutton->panel);
#endif

    return NULL;
}

wxSize wxSpinCtrlGTKBase::DoGetBestSize() const
{
    return DoGetSizeFromTextSize(95); // TODO: 95 is completely arbitrary
}

wxSize wxSpinCtrlGTKBase::DoGetSizeFromTextSize(int xlen, int ylen) const
{
    wxASSERT_MSG( m_widget, wxS("GetSizeFromTextSize called before creation") );

    // Set an as small as possible size for the control, so preferred sizes
    // return "natural" sizes, not taking into account the previous ones (which
    // seems to be GTK+3 behaviour)
    gtk_widget_set_size_request(m_widget, 0, 0);

    // Both Gtk+2 and Gtk+3 use current value/range to measure control's width.
    // So, we can't ask Gtk+ for its width. Instead, we used hardcoded values.

    // Returned height is OK
    wxSize totalS = GTKGetPreferredSize(m_widget);

#if GTK_CHECK_VERSION(3,4,0)
    // two buttons in horizontal
    totalS.x = 46 + 15; // margins included
#else
    // two small buttons in vertical
    totalS.x = GetFont().GetPixelSize().y + 13; // margins included
#endif

    wxSize tsize(xlen + totalS.x, totalS.y);

    // Check if the user requested a non-standard height.
    if ( ylen > 0 )
        tsize.IncBy(0, ylen - GetCharHeight());

    return tsize;
}

// static
wxVisualAttributes
wxSpinCtrlGTKBase::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_spin_button_new_with_range(0, 100, 1), true);
}

//-----------------------------------------------------------------------------
// wxSpinCtrl
//-----------------------------------------------------------------------------

extern "C"
{

static gboolean
wx_gtk_spin_input(GtkSpinButton* spin, gdouble* val, wxSpinCtrl* win)
{
    // We might use g_ascii_strtoll() here but it's 2.12+ only, so use our own
    // wxString function even if this requires an extra conversion.
    const wxString
        text(wxString::FromUTF8(gtk_entry_get_text(GTK_ENTRY(spin))));

    long lval;
    if ( !text.ToLong(&lval, win->GetBase()) )
        return FALSE;

    *val = lval;

    return TRUE;
}

static gint
wx_gtk_spin_output(GtkSpinButton* spin, wxSpinCtrl* win)
{
    const gint val = gtk_spin_button_get_value_as_int(spin);

    gtk_entry_set_text
    (
        GTK_ENTRY(spin),
        wxPrivate::wxSpinCtrlFormatAsHex(val, win->GetMax()).utf8_str()
    );

    return TRUE;
}

} // extern "C"

bool wxSpinCtrl::SetBase(int base)
{
    // Currently we only support base 10 and 16. We could add support for base
    // 8 quite easily but wxMSW doesn't support it natively so don't bother
    // with doing something wxGTK-specific here.
    if ( base != 10 && base != 16 )
        return false;

    if ( base == m_base )
        return true;

    m_base = base;

    // We need to be able to enter letters for any base greater than 10.
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON(m_widget), m_base <= 10 );

    if ( m_base != 10 )
    {
        g_signal_connect( m_widget, "input",
                              G_CALLBACK(wx_gtk_spin_input), this);
        g_signal_connect( m_widget, "output",
                              G_CALLBACK(wx_gtk_spin_output), this);
    }
    else
    {
        g_signal_handlers_disconnect_by_func(m_widget,
                                             (gpointer)wx_gtk_spin_input,
                                             this);
        g_signal_handlers_disconnect_by_func(m_widget,
                                             (gpointer)wx_gtk_spin_output,
                                             this);
    }

    return true;
}

//-----------------------------------------------------------------------------
// wxSpinCtrlDouble
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxSpinCtrlDouble, wxSpinCtrlGTKBase);

unsigned wxSpinCtrlDouble::GetDigits() const
{
    wxCHECK_MSG( m_widget, 0, "invalid spin button" );

    return gtk_spin_button_get_digits( GTK_SPIN_BUTTON(m_widget) );
}

void wxSpinCtrlDouble::SetDigits(unsigned digits)
{
    wxCHECK_RET( m_widget, "invalid spin button" );

    wxSpinCtrlEventDisabler disable(this);
    gtk_spin_button_set_digits( GTK_SPIN_BUTTON(m_widget), digits);
}

#endif // wxUSE_SPINCTRL
