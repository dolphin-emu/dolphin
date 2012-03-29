/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/tooltip.cpp
// Purpose:     wxToolTip implementation
// Author:      Robert Roebling
// Id:          $Id: tooltip.cpp 67326 2011-03-28 06:27:49Z PC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_TOOLTIPS

#include "wx/tooltip.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
#endif

#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// global data
//-----------------------------------------------------------------------------

#if !GTK_CHECK_VERSION(3,0,0) && !defined(GTK_DISABLE_DEPRECATED)
static GtkTooltips *gs_tooltips = NULL;
#endif

//-----------------------------------------------------------------------------
// wxToolTip
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxToolTip, wxObject)

wxToolTip::wxToolTip( const wxString &tip )
    : m_text(tip)
{
    m_window = NULL;
}

void wxToolTip::SetTip( const wxString &tip )
{
    m_text = tip;
    if (m_window)
        m_window->GTKApplyToolTip(wxGTK_CONV_SYS(m_text));
}

void wxToolTip::GTKSetWindow(wxWindow* win)
{
    wxASSERT(win);
    m_window = win;
    m_window->GTKApplyToolTip(wxGTK_CONV_SYS(m_text));
}

/* static */
void wxToolTip::GTKApply(GtkWidget* widget, const char* tip)
{
#if GTK_CHECK_VERSION(2, 12, 0)
    if (GTK_CHECK_VERSION(3,0,0) || gtk_check_version(2,12,0) == NULL)
        gtk_widget_set_tooltip_text(widget, tip);
    else
#endif
    {
#if !GTK_CHECK_VERSION(3,0,0) && !defined(GTK_DISABLE_DEPRECATED)
        if ( !gs_tooltips )
            gs_tooltips = gtk_tooltips_new();

        gtk_tooltips_set_tip(gs_tooltips, widget, tip, NULL);
#endif
    }
}

void wxToolTip::Enable( bool flag )
{
#if GTK_CHECK_VERSION(2, 12, 0)
    if (GTK_CHECK_VERSION(3,0,0) || gtk_check_version(2,12,0) == NULL)
    {
        GtkSettings* settings = gtk_settings_get_default();
        if (settings)
            gtk_settings_set_long_property(settings, "gtk-enable-tooltips", flag, NULL);
    }
    else
#endif
    {
#if !GTK_CHECK_VERSION(3,0,0) && !defined(GTK_DISABLE_DEPRECATED)
        if (!gs_tooltips)
            gs_tooltips = gtk_tooltips_new();

        if (flag)
            gtk_tooltips_enable( gs_tooltips );
        else
            gtk_tooltips_disable( gs_tooltips );
#endif
    }
}

void wxToolTip::SetDelay( long msecs )
{
#if GTK_CHECK_VERSION(2, 12, 0)
    if (GTK_CHECK_VERSION(3,0,0) || gtk_check_version(2,12,0) == NULL)
    {
        GtkSettings* settings = gtk_settings_get_default();
        if (settings)
            gtk_settings_set_long_property(settings, "gtk-tooltip-timeout", msecs, NULL);
    }
    else
#endif
    {
#if !GTK_CHECK_VERSION(3,0,0) && !defined(GTK_DISABLE_DEPRECATED)
        if (!gs_tooltips)
            gs_tooltips = gtk_tooltips_new();

        gtk_tooltips_set_delay( gs_tooltips, (int)msecs );
#endif
    }
}

void wxToolTip::SetAutoPop( long WXUNUSED(msecs) )
{
}

void wxToolTip::SetReshow( long WXUNUSED(msecs) )
{
}

#endif // wxUSE_TOOLTIPS
