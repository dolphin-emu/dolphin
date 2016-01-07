///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/activityindicator.cpp
// Purpose:     wxActivityIndicator implementation for wxGTK.
// Author:      Vadim Zeitlin
// Created:     2015-03-05
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ACTIVITYINDICATOR && defined(__WXGTK220__)

#include "wx/activityindicator.h"

#include "wx/math.h"

#include <gtk/gtk.h>

// Macro return the specified expression only if GTK+ run time version is less
// than 2.20 and compiling it only if it is less than 3.0 (which is why this
// has to be a macro and not a function).
#if defined(__WXGTK220__) && !defined(__WXGTK3__)
    #define RETURN_IF_NO_GTK_SPINNER(expr) \
        if ( gtk_check_version(2, 20, 0) != 0 ) { return expr; }
#else
    #define RETURN_IF_NO_GTK_SPINNER(expr)
#endif

// ============================================================================
// implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxActivityIndicator, wxControl);

bool
wxActivityIndicator::Create(wxWindow* parent,
                            wxWindowID winid,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxString& name)
{
    RETURN_IF_NO_GTK_SPINNER(
        wxActivityIndicatorGeneric::Create(parent, winid, pos, size, style, name)
    )

    if ( !PreCreation(parent, pos, size) ||
             !CreateBase(parent, winid, pos, size, style, name) )
        return false;

    m_widget = gtk_spinner_new();
    g_object_ref(m_widget);

    m_parent->DoAddChild(this);

    PostCreation(size);

    return true;
}

void wxActivityIndicator::Start()
{
    RETURN_IF_NO_GTK_SPINNER( wxActivityIndicatorGeneric::Start() )

    wxCHECK_RET( m_widget, wxS("Must be created first") );

    gtk_spinner_start(GTK_SPINNER(m_widget));
}

void wxActivityIndicator::Stop()
{
    RETURN_IF_NO_GTK_SPINNER( wxActivityIndicatorGeneric::Stop() )

    wxCHECK_RET( m_widget, wxS("Must be created first") );

    gtk_spinner_stop(GTK_SPINNER(m_widget));
}

bool wxActivityIndicator::IsRunning() const
{
    RETURN_IF_NO_GTK_SPINNER( wxActivityIndicatorGeneric::IsRunning() )

    if ( !m_widget )
        return false;

    gboolean b;
    g_object_get(m_widget, "active", &b, NULL);

    return b != 0;
}

wxSize wxActivityIndicator::DoGetBestClientSize() const
{
    RETURN_IF_NO_GTK_SPINNER( wxActivityIndicatorGeneric::DoGetBestClientSize() )

    if ( !m_widget )
        return wxDefaultSize;

    gint w, h;

#ifdef __WXGTK3__
    // gtk_widget_get_preferred_size() seems to return the size based on the
    // current size of the widget and also always returns 0 if it is hidden,
    // so ask GtkSpinner for its preferred size directly instead of using it.
    GtkWidgetClass* const wc = GTK_WIDGET_GET_CLASS(m_widget);

    // We're not interested in the natural size (and it's the same as minimal
    // one anyhow currently), but we still need a non-NULL pointer for it.
    gint dummy;
    wc->get_preferred_width(m_widget, &w, &dummy);
    wc->get_preferred_height(m_widget, &h, &dummy);
#else // GTK+ 2
    // GTK+ 2 doesn't return any valid preferred size for this control, so we
    // use the size of something roughly equivalent to it.
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
#endif // GTK+ 3/2

    // Adjust for the window variant: note that the default size in GTK+ 3 is
    // really small (12px until ~3.10, 16px since then), so we use this size as
    // the small size and double it for the normal size.
    double factor = 0.;
    switch ( GetWindowVariant() )
    {
        case wxWINDOW_VARIANT_MAX:
            wxFAIL_MSG(wxS("Invalid window variant"));
            wxFALLTHROUGH;

        case wxWINDOW_VARIANT_NORMAL:
            factor = 2.;
            break;

        case wxWINDOW_VARIANT_SMALL:
            factor = 1.;
            break;

        case wxWINDOW_VARIANT_MINI:
            factor = 0.75;
            break;

        case wxWINDOW_VARIANT_LARGE:
            // GTK+ 3.11+ limits GtkSpinner size to twice its minimal size, so
            // the effective factor here is actually just 2, i.e. the same as
            // for the normal size, but use something larger just in case GTK+
            // changes its mind again later.
            factor = 2.5;
            break;
    }

    wxASSERT_MSG( !wxIsSameDouble(factor, 0), wxS("Unknown window variant") );

    return wxSize(wxRound(w*factor), wxRound(h*factor));
}

#endif // wxUSE_ACTIVITYINDICATOR
