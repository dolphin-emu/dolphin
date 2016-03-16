/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/fontdlg.cpp
// Purpose:     wxFontDialog
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FONTDLG && !defined(__WXGPE__)

#include "wx/fontdlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
#endif

#include "wx/fontutil.h"
#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// "response"
//-----------------------------------------------------------------------------

extern "C" {
static void response(GtkDialog* dialog, int response_id, wxFontDialog* win)
{
    int rc = wxID_CANCEL;
    if (response_id == GTK_RESPONSE_OK)
    {
        rc = wxID_OK;
#if GTK_CHECK_VERSION(3,2,0)
        if (gtk_check_version(3,2,0) == NULL)
        {
            wxNativeFontInfo info;
            info.description = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(dialog));
            win->GetFontData().SetChosenFont(wxFont(info));
        }
        else
#endif
        {
            wxGCC_WARNING_SUPPRESS(deprecated-declarations)
            GtkFontSelectionDialog* sel = GTK_FONT_SELECTION_DIALOG(dialog);
            wxGtkString name(gtk_font_selection_dialog_get_font_name(sel));
            win->GetFontData().SetChosenFont(wxFont(wxString::FromUTF8(name)));
            wxGCC_WARNING_RESTORE()
        }
    }

    if (win->IsModal())
        win->EndModal(rc);
    else
        win->Show(false);
}
}

//-----------------------------------------------------------------------------
// wxFontDialog
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxFontDialog, wxDialog);

bool wxFontDialog::DoCreate(wxWindow *parent)
{
    parent = GetParentForModalDialog(parent, 0);

    if (!PreCreation( parent, wxDefaultPosition, wxDefaultSize ) ||
        !CreateBase( parent, -1, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE,
                     wxDefaultValidator, wxT("fontdialog") ))
    {
        wxFAIL_MSG( wxT("wxFontDialog creation failed") );
        return false;
    }

    const wxString message(_("Choose font"));
    GtkWindow* gtk_parent = NULL;
    if (parent)
        gtk_parent = GTK_WINDOW(parent->m_widget);

#if GTK_CHECK_VERSION(3,2,0)
#if GLIB_CHECK_VERSION(2, 34, 0)
    g_type_ensure(PANGO_TYPE_FONT_FACE);
#endif
    if (gtk_check_version(3,2,0) == NULL)
        m_widget = gtk_font_chooser_dialog_new(wxGTK_CONV(message), gtk_parent);
    else
#endif
    {
        wxGCC_WARNING_SUPPRESS(deprecated-declarations)
        m_widget = gtk_font_selection_dialog_new(wxGTK_CONV(message));
        if (gtk_parent)
            gtk_window_set_transient_for(GTK_WINDOW(m_widget), gtk_parent);
        wxGCC_WARNING_RESTORE()
    }
    g_object_ref(m_widget);

    g_signal_connect(m_widget, "response", G_CALLBACK(response), this);

    wxFont font = m_fontData.GetInitialFont();
    if( font.IsOk() )
    {
        const wxNativeFontInfo *info = font.GetNativeFontInfo();

        if ( info )
        {
#if GTK_CHECK_VERSION(3,2,0)
            if (gtk_check_version(3,2,0) == NULL)
                gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(m_widget), info->description);
            else
#endif
            {
                wxGCC_WARNING_SUPPRESS(deprecated-declarations)
                const wxString& fontname = info->ToString();
                GtkFontSelectionDialog* sel = GTK_FONT_SELECTION_DIALOG(m_widget);
                gtk_font_selection_dialog_set_font_name(sel, wxGTK_CONV(fontname));
                wxGCC_WARNING_RESTORE()
            }
        }
        else
        {
            // this is not supposed to happen!
            wxFAIL_MSG(wxT("font is ok but no native font info?"));
        }
    }

    return true;
}

wxFontDialog::~wxFontDialog()
{
}

#endif // wxUSE_FONTDLG && !__WXGPE__
