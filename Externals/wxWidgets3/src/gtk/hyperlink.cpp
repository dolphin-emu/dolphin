/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/hyperlink.cpp
// Purpose:     Hyperlink control
// Author:      Francesco Montorsi
// Created:     14/2/2007
// RCS-ID:      $Id: hyperlink.cpp 65334 2010-08-17 16:55:32Z VZ $
// Copyright:   (c) 2007 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// --------------------------------------------------------------------------
// headers
// --------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HYPERLINKCTRL && defined(__WXGTK210__) && !defined(__WXUNIVERSAL__)

#include "wx/hyperlink.h"

#ifndef WX_PRECOMP
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

// ----------------------------------------------------------------------------
// local functions
// ----------------------------------------------------------------------------

static inline bool UseNative()
{
    // native gtk_link_button widget is only available in GTK+ 2.10 and later
    return !gtk_check_version(2, 10, 0);
}

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// "clicked"
// ----------------------------------------------------------------------------

extern "C" {
static void gtk_hyperlink_clicked_callback( GtkWidget *WXUNUSED(widget),
                                            wxHyperlinkCtrl *linkCtrl )
{
    // send the event
    linkCtrl->SendEvent();
}
}

// ----------------------------------------------------------------------------
// wxHyperlinkCtrl
// ----------------------------------------------------------------------------

bool wxHyperlinkCtrl::Create(wxWindow *parent, wxWindowID id,
    const wxString& label, const wxString& url, const wxPoint& pos,
    const wxSize& size, long style, const wxString& name)
{
    if ( UseNative() )
    {
        // do validation checks:
        CheckParams(label, url, style);

        if (!PreCreation( parent, pos, size ) ||
            !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
        {
            wxFAIL_MSG( wxT("wxHyperlinkCtrl creation failed") );
            return false;
        }

        m_widget = gtk_link_button_new("asdfsaf asdfdsaf asdfdsa");
        g_object_ref(m_widget);
        gtk_widget_show(m_widget);

        // alignment
        float x_alignment = 0.5;
        if (HasFlag(wxHL_ALIGN_LEFT))
            x_alignment = 0.0;
        else if (HasFlag(wxHL_ALIGN_RIGHT))
            x_alignment = 1.0;
        gtk_button_set_alignment(GTK_BUTTON(m_widget), x_alignment, 0.5);

        // set to non empty strings both the url and the label
        SetURL(url.empty() ? label : url);
        SetLabel(label.empty() ? url : label);

        // our signal handlers:
        g_signal_connect_after (m_widget, "clicked",
                                G_CALLBACK (gtk_hyperlink_clicked_callback),
                                this);

        m_parent->DoAddChild( this );

        PostCreation(size);

        // wxWindowGTK will connect to the enter_notify and leave_notify GTK+ signals
        // thus overriding GTK+'s internal signal handlers which set the cursor of
        // the widget - thus we need to manually set it here:
        SetCursor(wxCursor(wxCURSOR_HAND));
    }
    else
        return wxGenericHyperlinkCtrl::Create(parent, id, label, url, pos, size, style, name);

    return true;
}

wxSize wxHyperlinkCtrl::DoGetBestSize() const
{
    if ( UseNative() )
        return wxControl::DoGetBestSize();
    return wxGenericHyperlinkCtrl::DoGetBestSize();
}

void wxHyperlinkCtrl::SetLabel(const wxString &label)
{
    if ( UseNative() )
    {
        wxControl::SetLabel(label);
        const wxString labelGTK = GTKConvertMnemonics(label);
        gtk_button_set_label(GTK_BUTTON(m_widget), wxGTK_CONV(labelGTK));
    }
    else
        wxGenericHyperlinkCtrl::SetLabel(label);
}

void wxHyperlinkCtrl::SetURL(const wxString &uri)
{
    if ( UseNative() )
        gtk_link_button_set_uri(GTK_LINK_BUTTON(m_widget), wxGTK_CONV(uri));
    else
        wxGenericHyperlinkCtrl::SetURL(uri);
}

wxString wxHyperlinkCtrl::GetURL() const
{
    if ( UseNative() )
    {
        const gchar *str = gtk_link_button_get_uri(GTK_LINK_BUTTON(m_widget));
        return wxString::FromUTF8(str);
    }

    return wxGenericHyperlinkCtrl::GetURL();
}

void wxHyperlinkCtrl::SetNormalColour(const wxColour &colour)
{
    if ( UseNative() )
    {
        // simply do nothing: GTK+ does not allow us to change it :(
    }
    else
        wxGenericHyperlinkCtrl::SetNormalColour(colour);
}

wxColour wxHyperlinkCtrl::GetNormalColour() const
{
    wxColour ret;
    if ( UseNative() )
    {
        GdkColor *link_color = NULL;

        // convert GdkColor in wxColour
        gtk_widget_style_get(m_widget, "link-color", &link_color, NULL);
        if (link_color)
            ret = wxColour(*link_color);
        gdk_color_free (link_color);
    }
    else
        ret = wxGenericHyperlinkCtrl::GetNormalColour();

    return ret;
}

void wxHyperlinkCtrl::SetVisitedColour(const wxColour &colour)
{
    if ( UseNative() )
    {
        // simply do nothing: GTK+ does not allow us to change it :(
    }
    else
        wxGenericHyperlinkCtrl::SetVisitedColour(colour);
}

wxColour wxHyperlinkCtrl::GetVisitedColour() const
{
    wxColour ret;
    if ( UseNative() )
    {
        GdkColor *link_color = NULL;

        // convert GdkColor in wxColour
        gtk_widget_style_get(m_widget, "visited-link-color", &link_color, NULL);
        if (link_color)
            ret = wxColour(*link_color);
        gdk_color_free (link_color);
    }
    else
        return wxGenericHyperlinkCtrl::GetVisitedColour();

    return ret;
}

void wxHyperlinkCtrl::SetHoverColour(const wxColour &colour)
{
    if ( UseNative() )
    {
        // simply do nothing: GTK+ does not allow us to change it :(
    }
    else
        wxGenericHyperlinkCtrl::SetHoverColour(colour);
}

wxColour wxHyperlinkCtrl::GetHoverColour() const
{
    if ( UseNative() )
    {
        // hover colour == normal colour for native GTK+ widget
        return GetNormalColour();
    }

    return wxGenericHyperlinkCtrl::GetHoverColour();
}

GdkWindow *wxHyperlinkCtrl::GTKGetWindow(wxArrayGdkWindows& windows) const
{
    return UseNative() ? GTK_BUTTON(m_widget)->event_window
                       : wxGenericHyperlinkCtrl::GTKGetWindow(windows);
}

#endif // wxUSE_HYPERLINKCTRL && GTK+ 2.10+
