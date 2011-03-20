/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/msgdlg.cpp
// Purpose:     wxMessageDialog for GTK+2
// Author:      Vaclav Slavik
// Modified by:
// Created:     2003/02/28
// RCS-ID:      $Id: msgdlg.cpp 65812 2010-10-15 23:45:58Z VZ $
// Copyright:   (c) Vaclav Slavik, 2003
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MSGDLG && !defined(__WXGPE__)

#include "wx/msgdlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
#endif

#include "wx/gtk/private.h"
#include "wx/gtk/private/messagetype.h"
#include "wx/gtk/private/mnemonics.h"
#include <gtk/gtk.h>

#if wxUSE_LIBHILDON
    #include <hildon-widgets/hildon-note.h>
#endif // wxUSE_LIBHILDON

#if wxUSE_LIBHILDON2
    #include <hildon/hildon.h>
#endif // wxUSE_LIBHILDON2

IMPLEMENT_CLASS(wxMessageDialog, wxDialog)

wxMessageDialog::wxMessageDialog(wxWindow *parent,
                                 const wxString& message,
                                 const wxString& caption,
                                 long style,
                                 const wxPoint& WXUNUSED(pos))
               : wxMessageDialogBase
                 (
                    GetParentForModalDialog(parent, style),
                    message,
                    caption,
                    style
                 )
{
}

wxString wxMessageDialog::GetDefaultYesLabel() const
{
    return GTK_STOCK_YES;
}

wxString wxMessageDialog::GetDefaultNoLabel() const
{
    return GTK_STOCK_NO;
}

wxString wxMessageDialog::GetDefaultOKLabel() const
{
    return GTK_STOCK_OK;
}

wxString wxMessageDialog::GetDefaultCancelLabel() const
{
    return GTK_STOCK_CANCEL;
}

void wxMessageDialog::DoSetCustomLabel(wxString& var, const ButtonLabel& label)
{
    int stockId = label.GetStockId();
    if ( stockId == wxID_NONE )
    {
        wxMessageDialogBase::DoSetCustomLabel(var, label);
        var = wxConvertMnemonicsToGTK(var);
    }
    else // stock label
    {
        var = wxGetStockGtkID(stockId);
    }
}

void wxMessageDialog::GTKCreateMsgDialog()
{
    GtkWindow * const parent = m_parent ? GTK_WINDOW(m_parent->m_widget) : NULL;

#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
    const char *stockIcon = "";

    switch ( GetEffectiveIcon() )
    {
        case wxICON_ERROR:
            stockIcon = "qgn_note_gene_syserror";
            break;

        case wxICON_WARNING:
            stockIcon = "qgn_note_gene_syswarning";
            break;

        case wxICON_QUESTION:
            stockIcon = "qgn_note_confirm";
            break;

        case wxICON_INFORMATION:
            stockIcon = "qgn_note_info";
            break;
    }

    // there is no generic note creation function in public API so we have no
    // choice but to use g_object_new() directly
    m_widget = (GtkWidget *)g_object_new
               (
                HILDON_TYPE_NOTE,
#if wxUSE_LIBHILDON
                "note_type", HILDON_NOTE_CONFIRMATION_BUTTON_TYPE,
#else // wxUSE_LIBHILDON
                "note_type", HILDON_NOTE_TYPE_CONFIRMATION_BUTTON,
#endif // wxUSE_LIBHILDON /wxUSE_LIBHILDON2
                "description", (const char *)GetFullMessage().utf8_str(),
                "icon", stockIcon,
                NULL
               );
#else // !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    GtkMessageType type = GTK_MESSAGE_ERROR;
    GtkButtonsType buttons = GTK_BUTTONS_NONE;

    // when using custom labels, we have to add all the buttons ourselves
    if ( !HasCustomLabels() )
    {
        if ( m_dialogStyle & wxYES_NO )
        {
            if ( !(m_dialogStyle & wxCANCEL) )
                buttons = GTK_BUTTONS_YES_NO;
            //else: no standard GTK_BUTTONS_YES_NO_CANCEL so leave as NONE
        }
        else if ( m_dialogStyle & wxOK )
        {
            buttons = m_dialogStyle & wxCANCEL ? GTK_BUTTONS_OK_CANCEL
                                               : GTK_BUTTONS_OK;
        }
    }

    if ( !wxGTKImpl::ConvertMessageTypeFromWX(GetEffectiveIcon(), &type) )
    {
        // if no style is explicitly specified, detect the suitable icon
        // ourselves (this can be disabled by using wxICON_NONE)
        type = m_dialogStyle & wxYES ? GTK_MESSAGE_QUESTION : GTK_MESSAGE_INFO;
    }

    wxString message;
#if GTK_CHECK_VERSION(2, 6, 0)
    bool needsExtMessage = false;
    if ( gtk_check_version(2, 6, 0) == NULL && !m_extendedMessage.empty() )
    {
        message = m_message;
        needsExtMessage = true;
    }
    else // extended message not needed or not supported
#endif // GTK+ 2.6+
    {
        message = GetFullMessage();
    }

    m_widget = gtk_message_dialog_new(parent,
                                      GTK_DIALOG_MODAL,
                                      type,
                                      buttons,
                                      "%s",
                                      (const char*)wxGTK_CONV(message));

#if GTK_CHECK_VERSION(2, 6, 0)
    if ( needsExtMessage )
    {
        gtk_message_dialog_format_secondary_text
        (
            (GtkMessageDialog *)m_widget,
            "%s",
            (const char *)wxGTK_CONV(m_extendedMessage)
        );
    }
#endif // GTK+ 2.6+
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2/!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2

    g_object_ref(m_widget);

    if (m_caption != wxMessageBoxCaptionStr)
        gtk_window_set_title(GTK_WINDOW(m_widget), wxGTK_CONV(m_caption));

    GtkDialog * const dlg = GTK_DIALOG(m_widget);

    if ( m_dialogStyle & wxSTAY_ON_TOP )
    {
        gtk_window_set_keep_above(GTK_WINDOW(m_widget), TRUE);
    }

    // we need to add buttons manually if we use custom labels or always for
    // Yes/No/Cancel dialog as GTK+ doesn't support it natively and when using
    // Hildon we add all the buttons manually as it doesn't support too many of
    // the combinations we may have
#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
    static const bool addButtons = true;
#else // !wxUSE_LIBHILDON
    const bool addButtons = buttons == GTK_BUTTONS_NONE;
#endif // wxUSE_LIBHILDON/!wxUSE_LIBHILDON

    if ( m_dialogStyle & wxYES_NO ) // Yes/No or Yes/No/Cancel dialog
    {
        if ( addButtons )
        {
            // Add the buttons in the correct order which is, according to
            // http://library.gnome.org/devel/hig-book/stable/windows-alert.html.en
            // the following one:
            //
            // [Help]                  [Alternative] [Cancel] [Affirmative]

            gtk_dialog_add_button(dlg, wxGTK_CONV(GetNoLabel()),
                                  GTK_RESPONSE_NO);

            if ( m_dialogStyle & wxCANCEL )
            {
                gtk_dialog_add_button(dlg, wxGTK_CONV(GetCancelLabel()),
                                      GTK_RESPONSE_CANCEL);
            }

            gtk_dialog_add_button(dlg, wxGTK_CONV(GetYesLabel()),
                                  GTK_RESPONSE_YES);
        }

        // it'd probably be harmless to call gtk_dialog_set_default_response()
        // twice but why do it if we're going to change the default below
        // anyhow
        if ( !(m_dialogStyle & wxCANCEL_DEFAULT) )
        {
            gtk_dialog_set_default_response(dlg,
                                            m_dialogStyle & wxNO_DEFAULT
                                                ? GTK_RESPONSE_NO
                                                : GTK_RESPONSE_YES);
        }
    }
    else if ( addButtons ) // Ok or Ok/Cancel dialog
    {
        gtk_dialog_add_button(dlg, wxGTK_CONV(GetOKLabel()), GTK_RESPONSE_OK);
        if ( m_dialogStyle & wxCANCEL )
        {
            gtk_dialog_add_button(dlg, wxGTK_CONV(GetCancelLabel()),
                                  GTK_RESPONSE_CANCEL);
        }
    }

    if ( m_dialogStyle & wxCANCEL_DEFAULT )
    {
        gtk_dialog_set_default_response(dlg, GTK_RESPONSE_CANCEL);
    }
}

int wxMessageDialog::ShowModal()
{
    // break the mouse capture as it would interfere with modal dialog (see
    // wxDialog::ShowModal)
    wxWindow * const win = wxWindow::GetCapture();
    if ( win )
        win->GTKReleaseMouseAndNotify();

    if ( !m_widget )
    {
        GTKCreateMsgDialog();
        wxCHECK_MSG( m_widget, wxID_CANCEL,
                     wxT("failed to create GtkMessageDialog") );
    }

    // This should be necessary, but otherwise the
    // parent TLW will disappear..
    if (m_parent)
        gtk_window_present( GTK_WINDOW(m_parent->m_widget) );

    gint result = gtk_dialog_run(GTK_DIALOG(m_widget));
    gtk_widget_destroy(m_widget);
    g_object_unref(m_widget);
    m_widget = NULL;

    switch (result)
    {
        default:
            wxFAIL_MSG(wxT("unexpected GtkMessageDialog return code"));
            // fall through

        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CLOSE:
            return wxID_CANCEL;
        case GTK_RESPONSE_OK:
            return wxID_OK;
        case GTK_RESPONSE_YES:
            return wxID_YES;
        case GTK_RESPONSE_NO:
            return wxID_NO;
    }
}


#endif // wxUSE_MSGDLG && !defined(__WXGPE__)
