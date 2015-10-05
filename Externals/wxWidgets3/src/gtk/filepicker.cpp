/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/filepicker.cpp
// Purpose:     implementation of wxFileButton and wxDirButton
// Author:      Francesco Montorsi
// Modified By:
// Created:     15/04/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FILEPICKERCTRL

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#include "wx/filepicker.h"
#include "wx/tooltip.h"

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

// ============================================================================
// implementation
// ============================================================================

//-----------------------------------------------------------------------------
// wxFileButton
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxFileButton, wxButton);

bool wxFileButton::Create( wxWindow *parent, wxWindowID id,
                        const wxString &label, const wxString &path,
                        const wxString &message, const wxString &wildcard,
                        const wxPoint &pos, const wxSize &size,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    // we can't use the native button for wxFLP_SAVE pickers as it can only
    // open existing files and there is no way to create a new file using it
    if (!(style & wxFLP_SAVE) && !(style & wxFLP_USE_TEXTCTRL))
    {
        // VERY IMPORTANT: this code is identical to relative code in wxDirButton;
        //                 if you find a problem here, fix it also in wxDirButton !

        if (!PreCreation( parent, pos, size ) ||
            !wxControl::CreateBase(parent, id, pos, size, style & wxWINDOW_STYLE_MASK,
                                    validator, name))
        {
            wxFAIL_MSG( wxT("wxFileButton creation failed") );
            return false;
        }

        // create the dialog associated with this button
        // NB: unlike generic implementation, native GTK implementation needs to create
        //     the filedialog here as it needs to use gtk_file_chooser_button_new_with_dialog()
        SetWindowStyle(style);
        m_path = path;
        m_message = message;
        m_wildcard = wildcard;
        if ((m_dialog = CreateDialog()) == NULL)
            return false;

        // little trick used to avoid problems when there are other GTK windows 'grabbed':
        // GtkFileChooserDialog won't be responsive to user events if there is another
        // window which called gtk_grab_add (and this happens if e.g. a wxDialog is running
        // in modal mode in the application - see wxDialogGTK::ShowModal).
        // An idea could be to put the grab on the m_dialog->m_widget when the GtkFileChooserButton
        // is clicked and then remove it as soon as the user closes the dialog itself.
        // Unfortunately there's no way to hook in the 'clicked' event of the GtkFileChooserButton,
        // thus we add grab on m_dialog->m_widget when it's shown and remove it when it's
        // hidden simply using its "show" and "hide" events - clean & simple :)
        g_signal_connect(m_dialog->m_widget, "show", G_CALLBACK(gtk_grab_add), NULL);
        g_signal_connect(m_dialog->m_widget, "hide", G_CALLBACK(gtk_grab_remove), NULL);

        //       use as label the currently selected file
        m_widget = gtk_file_chooser_button_new_with_dialog( m_dialog->m_widget );
        g_object_ref(m_widget);

        // we need to know when the dialog has been dismissed clicking OK...
        // NOTE: the "clicked" signal is not available for a GtkFileChooserButton
        //       thus we are forced to use wxFileDialog's event
        m_dialog->Connect(wxEVT_BUTTON,
                wxCommandEventHandler(wxFileButton::OnDialogOK),
                NULL, this);

        m_parent->DoAddChild( this );

        PostCreation(size);
        SetInitialSize(size);
    }
    else
        return wxGenericFileButton::Create(parent, id, label, path, message, wildcard,
                                           pos, size, style, validator, name);
    return true;
}

wxFileButton::~wxFileButton()
{
    if ( m_dialog )
    {
        // when m_dialog is deleted, it will destroy the widget it is sharing
        // with GtkFileChooserButton, which results in a bunch of Gtk-CRITICAL
        // errors from GtkFileChooserButton. To avoid this, call gtk_widget_destroy()
        // on GtkFileChooserButton first (our base dtor will do it again, but
        // that does no harm). m_dialog holds a reference to the shared widget,
        // so it won't go away until m_dialog base dtor unrefs it.
        gtk_widget_destroy(m_widget);
        delete m_dialog;
    }
}

void wxFileButton::OnDialogOK(wxCommandEvent& ev)
{
    // the wxFileDialog associated with the GtkFileChooserButton has been closed
    // using the OK button, thus the selected file has changed...
    if (ev.GetId() == wxID_OK)
    {
        // ...update our path
        UpdatePathFromDialog(m_dialog);

        // ...and fire an event
        wxFileDirPickerEvent event(wxEVT_FILEPICKER_CHANGED, this, GetId(), m_path);
        HandleWindowEvent(event);
    }
}

void wxFileButton::SetPath(const wxString &str)
{
    m_path = str;

    if (m_dialog)
        UpdateDialogPath(m_dialog);
}

void wxFileButton::SetInitialDirectory(const wxString& dir)
{
    if (m_dialog)
    {
        // Only change the directory if the default file name doesn't have any
        // directory in it, otherwise it takes precedence.
        if ( m_path.find_first_of(wxFileName::GetPathSeparators()) ==
                wxString::npos )
        {
            static_cast<wxFileDialog*>(m_dialog)->SetDirectory(dir);
        }
    }
    else
        wxGenericFileButton::SetInitialDirectory(dir);
}

#endif // wxUSE_FILEPICKERCTRL

#if wxUSE_DIRPICKERCTRL

#ifdef __UNIX__
#include <unistd.h> // chdir
#endif

//-----------------------------------------------------------------------------
// "file-set"
//-----------------------------------------------------------------------------

extern "C" {
static void file_set(GtkFileChooser* widget, wxDirButton* p)
{
    // NB: it's important to use gtk_file_chooser_get_filename instead of
    //     gtk_file_chooser_get_current_folder (see GTK docs) !
    wxGtkString filename(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget)));
    p->GTKUpdatePath(filename);

    // since GtkFileChooserButton when used to pick directories also uses a combobox,
    // maybe that the current folder has been changed but not through the GtkFileChooserDialog
    // and thus the 'gtk_filedialog_ok_callback' could have not been called...
    // thus we need to make sure the current working directory is updated if wxDIRP_CHANGE_DIR
    // style was given.
    if (p->HasFlag(wxDIRP_CHANGE_DIR))
    {
        if ( chdir(filename) != 0 )
        {
            wxLogSysError(_("Changing current directory to \"%s\" failed"),
                          wxString::FromUTF8(filename));
        }
    }

    // ...and fire an event
    wxFileDirPickerEvent event(wxEVT_DIRPICKER_CHANGED, p, p->GetId(), p->GetPath());
    p->HandleWindowEvent(event);
}
}

//-----------------------------------------------------------------------------
// "selection-changed"
//-----------------------------------------------------------------------------

extern "C" {
static void selection_changed(GtkFileChooser* chooser, wxDirButton* win)
{
    char* filename = gtk_file_chooser_get_filename(chooser);

    if (wxString::FromUTF8(filename) == win->GetPath())
        win->m_bIgnoreNextChange = false;
    else if (!win->m_bIgnoreNextChange)
        file_set(chooser, win);

    g_free(filename);
}
}

//-----------------------------------------------------------------------------
// wxDirButtonGTK
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxDirButton, wxButton);

bool wxDirButton::Create( wxWindow *parent, wxWindowID id,
                        const wxString &label, const wxString &path,
                        const wxString &message, const wxString &wildcard,
                        const wxPoint &pos, const wxSize &size,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    if (!(style & wxDIRP_USE_TEXTCTRL))
    {
        // VERY IMPORTANT: this code is identic to relative code in wxFileButton;
        //                 if you find a problem here, fix it also in wxFileButton !

        if (!PreCreation( parent, pos, size ) ||
            !wxControl::CreateBase(parent, id, pos, size, style & wxWINDOW_STYLE_MASK,
                                    validator, name))
        {
            wxFAIL_MSG( wxT("wxDirButtonGTK creation failed") );
            return false;
        }

        // create the dialog associated with this button
        SetWindowStyle(style);
        m_message = message;
        m_wildcard = wildcard;
        if ((m_dialog = CreateDialog()) == NULL)
            return false;

        // little trick used to avoid problems when there are other GTK windows 'grabbed':
        // GtkFileChooserDialog won't be responsive to user events if there is another
        // window which called gtk_grab_add (and this happens if e.g. a wxDialog is running
        // in modal mode in the application - see wxDialogGTK::ShowModal).
        // An idea could be to put the grab on the m_dialog->m_widget when the GtkFileChooserButton
        // is clicked and then remove it as soon as the user closes the dialog itself.
        // Unfortunately there's no way to hook in the 'clicked' event of the GtkFileChooserButton,
        // thus we add grab on m_dialog->m_widget when it's shown and remove it when it's
        // hidden simply using its "show" and "hide" events - clean & simple :)
        g_signal_connect(m_dialog->m_widget, "show", G_CALLBACK(gtk_grab_add), NULL);
        g_signal_connect(m_dialog->m_widget, "hide", G_CALLBACK(gtk_grab_remove), NULL);


        // NOTE: we deliberately ignore the given label as GtkFileChooserButton
        //       use as label the currently selected file
        m_widget = gtk_file_chooser_button_new_with_dialog( m_dialog->m_widget );
        g_object_ref(m_widget);
        SetPath(path);

#ifdef __WXGTK3__
        if (gtk_check_version(3,8,0) == NULL)
            g_signal_connect(m_widget, "file_set", G_CALLBACK(file_set), this);
        else
#endif
        {
            // prior to GTK+ 3.8 neither "file-set" nor "current-folder-changed" will be
            // emitted when the user selects one of the special folders from the combobox
            g_signal_connect(m_widget, "selection_changed",
                G_CALLBACK(selection_changed), this);
        }

        m_parent->DoAddChild( this );

        PostCreation(size);
        SetInitialSize(size);
    }
    else
        return wxGenericDirButton::Create(parent, id, label, path, message, wildcard,
                                          pos, size, style, validator, name);
    return true;
}

wxDirButton::~wxDirButton()
{
    if (m_dialog)
    {
        // see ~wxFileButton() comment
        gtk_widget_destroy(m_widget);
        delete m_dialog;
    }
}

void wxDirButton::GTKUpdatePath(const char *gtkpath)
{
    m_path = wxString::FromUTF8(gtkpath);
}
void wxDirButton::SetPath(const wxString& str)
{
    if ( m_path == str )
        return;

    m_path = str;

    m_bIgnoreNextChange = true;

    if (GTK_IS_FILE_CHOOSER(m_widget))
        gtk_file_chooser_set_filename((GtkFileChooser*)m_widget, str.utf8_str());
    else if (m_dialog)
        UpdateDialogPath(m_dialog);
}

void wxDirButton::SetInitialDirectory(const wxString& dir)
{
    if (m_dialog)
    {
        if (m_path.empty())
            static_cast<wxDirDialog*>(m_dialog)->SetPath(dir);
    }
    else
        wxGenericDirButton::SetInitialDirectory(dir);
}

#endif // wxUSE_DIRPICKERCTRL
