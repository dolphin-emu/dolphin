///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/hildon/notifmsg.cpp
// Purpose:     Hildon implementation of wxNotificationMessage
// Author:      Vadim Zeitlin
// Created:     2007-11-21
// RCS-ID:      $Id: notifmsg.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
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

#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#include "wx/notifmsg.h"
#include "wx/toplevel.h"

#if wxUSE_LIBHILDON
    #include <hildon-widgets/hildon-banner.h>
#endif // wxUSE_LIBHILDON

#if wxUSE_LIBHILDON2
    #include <hildon/hildon.h>
#endif // wxUSE_LIBHILDON2

// ============================================================================
// wxNotificationMessage implementation
// ============================================================================

wxString wxNotificationMessage::HildonGetMarkup() const
{
    const wxString& message = GetMessage(),
                    title = GetTitle();

    wxString text;
    if ( message.empty() )
    {
        text = title;
    }
    else // combine title with message in a single string
    {
        text << "<big><b>" << title << "</b></big>\n"
                "\n"
             << message;
    }

    return text;
}

GtkWidget *wxNotificationMessage::HildonGetWindow() const
{
    wxWindow *parent = GetParent();
    if ( parent )
    {
        parent = wxGetTopLevelParent(parent);
        if ( parent )
        {
            wxTopLevelWindow * const
                tlw = wxDynamicCast(parent, wxTopLevelWindow);
            if ( tlw )
                return tlw->m_mainWidget;
        }
    }

    return NULL;
}

bool wxNotificationMessage::Show(int timeout)
{
    if ( timeout == Timeout_Never )
    {
        m_banner = hildon_banner_show_animation
                   (
                    HildonGetWindow(),
                    NULL,
                    GetFullMessage() // markup not supported here
                   );
        if ( !m_banner )
            return false;
    }
    else // the message will time out
    {
        // we don't have any way to set the timeout interval so we just let it
        // time out automatically
        hildon_banner_show_information_with_markup
        (
            HildonGetWindow(),
            NULL,
            HildonGetMarkup()
        );
    }

    return true;
}

bool wxNotificationMessage::Close()
{
    if ( !m_banner )
    {
        // either we hadn't been shown or we are using an information banner
        // which will disappear on its own, nothing we can do about it
        return false;
    }

    gtk_widget_destroy(m_banner);
    m_banner = NULL;

    return true;
}

wxNotificationMessage::~wxNotificationMessage()
{
    Close();
}

#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2
