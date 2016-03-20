///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/aboutdlg.cpp
// Purpose:     native GTK+ wxAboutBox() implementation
// Author:      Vadim Zeitlin
// Created:     2006-10-08
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
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

#if wxUSE_ABOUTDLG

#include "wx/aboutdlg.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
#endif //WX_PRECOMP

#include <gtk/gtk.h>
#include "wx/gtk/private.h"
#include "wx/gtk/private/gtk2-compat.h"

// ----------------------------------------------------------------------------
// GtkArray: temporary array of GTK strings
// ----------------------------------------------------------------------------

namespace
{

class GtkArray
{
public:
    // Create empty GtkArray
    GtkArray() : m_strings(0), m_count(0)
    {
    }

    // Create GtkArray from wxArrayString. Note that the created object is
    // only valid as long as 'a' is!
    GtkArray(const wxArrayString& a)
    {
        m_count = a.size();
        m_strings = new const gchar *[m_count + 1];

        for ( size_t n = 0; n < m_count; n++ )
        {
#if wxUSE_UNICODE
            // notice that there is no need to copy the string pointer here
            // because this class is used only as a temporary and during its
            // existence the pointer persists in wxString which uses it either
            // for internal representation (in wxUSE_UNICODE_UTF8 case) or as
            // cached m_convertedToChar (in wxUSE_UNICODE_WCHAR case)
            m_strings[n] = wxGTK_CONV_SYS(a[n]);
#else // !wxUSE_UNICODE
            // and in ANSI build we can simply borrow the pointer from
            // wxCharBuffer (which owns it in this case) instead of copying it
            // but we then become responsible for freeing it
            m_strings[n] = wxGTK_CONV_SYS(a[n]).release();
#endif // wxUSE_UNICODE/!wxUSE_UNICODE
        }

        // array must be NULL-terminated
        m_strings[m_count] = NULL;
    }

    operator const gchar **() const { return m_strings; }

    ~GtkArray()
    {
#if !wxUSE_UNICODE
        for ( size_t n = 0; n < m_count; n++ )
            free(const_cast<gchar *>(m_strings[n]));
#endif

        delete [] m_strings;
    }

private:
    const gchar **m_strings;
    size_t m_count;

    wxDECLARE_NO_COPY_CLASS(GtkArray);
};

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

// GTK+ about dialog is modeless, keep track of it in this variable
static GtkAboutDialog *gs_aboutDialog = NULL;

extern "C" {
static void wxGtkAboutDialogOnClose(GtkAboutDialog *about)
{
    gtk_widget_destroy(GTK_WIDGET(about));
    if ( about == gs_aboutDialog )
        gs_aboutDialog = NULL;
}
}

#ifdef __WXGTK3__
extern "C" {
static gboolean activate_link(GtkAboutDialog*, const char* link, void* dontIgnore)
{
    if (dontIgnore)
    {
        wxLaunchDefaultBrowser(wxGTK_CONV_BACK_SYS(link));
        return true;
    }
    return false;
}
}
#else
extern "C" {
static void wxGtkAboutDialogOnLink(GtkAboutDialog*, const char* link, void*)
{
    wxLaunchDefaultBrowser(wxGTK_CONV_BACK_SYS(link));
}
}
#endif

void wxAboutBox(const wxAboutDialogInfo& info, wxWindow* parent)
{
    // don't create another dialog if one is already present
    if ( !gs_aboutDialog )
        gs_aboutDialog = GTK_ABOUT_DIALOG(gtk_about_dialog_new());

    GtkAboutDialog * const dlg = gs_aboutDialog;
    gtk_about_dialog_set_program_name(dlg, wxGTK_CONV_SYS(info.GetName()));
    if ( info.HasVersion() )
        gtk_about_dialog_set_version(dlg, wxGTK_CONV_SYS(info.GetVersion()));
    else
        gtk_about_dialog_set_version(dlg, NULL);
    if ( info.HasCopyright() )
        gtk_about_dialog_set_copyright(dlg, wxGTK_CONV_SYS(info.GetCopyrightToDisplay()));
    else
        gtk_about_dialog_set_copyright(dlg, NULL);
    if ( info.HasDescription() )
        gtk_about_dialog_set_comments(dlg, wxGTK_CONV_SYS(info.GetDescription()));
    else
        gtk_about_dialog_set_comments(dlg, NULL);
    if ( info.HasLicence() )
        gtk_about_dialog_set_license(dlg, wxGTK_CONV_SYS(info.GetLicence()));
    else
        gtk_about_dialog_set_license(dlg, NULL);

    wxIcon icon = info.GetIcon();
    if ( icon.IsOk() )
        gtk_about_dialog_set_logo(dlg, info.GetIcon().GetPixbuf());

    if ( info.HasWebSite() )
    {
#ifdef __WXGTK3__
        g_signal_connect(dlg, "activate-link", G_CALLBACK(activate_link), dlg);
#else
        // NB: must be called before gtk_about_dialog_set_website() as
        //     otherwise it has no effect (although GTK+ docs don't mention
        //     this...)
        gtk_about_dialog_set_url_hook(wxGtkAboutDialogOnLink, NULL, NULL);
#endif

        gtk_about_dialog_set_website(dlg, wxGTK_CONV_SYS(info.GetWebSiteURL()));
        gtk_about_dialog_set_website_label
        (
            dlg,
            wxGTK_CONV_SYS(info.GetWebSiteDescription())
        );
    }
    else
    {
        gtk_about_dialog_set_website(dlg, NULL);
        gtk_about_dialog_set_website_label(dlg, NULL);
#ifdef __WXGTK3__
        g_signal_connect(dlg, "activate-link", G_CALLBACK(activate_link), NULL);
#else
        gtk_about_dialog_set_url_hook(NULL, NULL, NULL);
#endif
    }

    if ( info.HasDevelopers() )
        gtk_about_dialog_set_authors(dlg, GtkArray(info.GetDevelopers()));
    else
        gtk_about_dialog_set_authors(dlg, GtkArray());
    if ( info.HasDocWriters() )
        gtk_about_dialog_set_documenters(dlg, GtkArray(info.GetDocWriters()));
    else
        gtk_about_dialog_set_documenters(dlg, GtkArray());
    if ( info.HasArtists() )
        gtk_about_dialog_set_artists(dlg, GtkArray(info.GetArtists()));
    else
        gtk_about_dialog_set_artists(dlg, GtkArray());

    wxString transCredits;
    if ( info.HasTranslators() )
    {
        const wxArrayString& translators = info.GetTranslators();
        const size_t count = translators.size();
        for ( size_t n = 0; n < count; n++ )
        {
            transCredits << translators[n] << wxT('\n');
        }
    }
    else // no translators explicitly specified
    {
        // maybe we have translator credits in the message catalog?
        wxString translator = _("translator-credits");

        // gtk_about_dialog_set_translator_credits() is smart enough to
        // detect if "translator-credits" is untranslated and hide the
        // translators tab in that case, however it will still show the
        // "credits" button, (at least GTK 2.10.6) even if there are no
        // credits informations at all, so we still need to do the check
        // ourselves
        if ( translator != wxT("translator-credits") ) // untranslated!
            transCredits = translator;
    }

    if ( !transCredits.empty() )
        gtk_about_dialog_set_translator_credits(dlg, wxGTK_CONV_SYS(transCredits));
    else
        gtk_about_dialog_set_translator_credits(dlg, NULL);

    g_signal_connect(dlg, "response",
                        G_CALLBACK(wxGtkAboutDialogOnClose), NULL);

    GtkWindow* gtkParent = NULL;
    if (parent && parent->m_widget)
        gtkParent = (GtkWindow*)gtk_widget_get_ancestor(parent->m_widget, GTK_TYPE_WINDOW);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), gtkParent);

    gtk_window_present(GTK_WINDOW(dlg));
}

#endif // wxUSE_ABOUTDLG
