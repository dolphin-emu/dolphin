/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/filepicker.cpp
// Purpose:     implementation of wxFileButton and wxDirButton
// Author:      Francesco Montorsi
// Modified By:
// Created:     15/04/2006
// Id:          $Id: filepicker.cpp 61724 2009-08-21 10:41:26Z VZ $
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FILEPICKERCTRL && defined(__WXGTK26__)

#include "wx/filepicker.h"
#include "wx/tooltip.h"

#include "wx/gtk/private.h"

// ============================================================================
// implementation
// ============================================================================

//-----------------------------------------------------------------------------
// wxFileButton
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxFileButton, wxButton)

bool wxFileButton::Create( wxWindow *parent, wxWindowID id,
                        const wxString &label, const wxString &path,
                        const wxString &message, const wxString &wildcard,
                        const wxPoint &pos, const wxSize &size,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    // we can't use the native button for wxFLP_SAVE pickers as it can only
    // open existing files and there is no way to create a new file using it
    if ( !(style & wxFLP_SAVE) && !(style & wxFLP_USE_TEXTCTRL) && !gtk_check_version(2,6,0) )
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
        gtk_widget_show(m_widget);

        // we need to know when the dialog has been dismissed clicking OK...
        // NOTE: the "clicked" signal is not available for a GtkFileChooserButton
        //       thus we are forced to use wxFileDialog's event
        m_dialog->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
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
        wxFileDirPickerEvent event(wxEVT_COMMAND_FILEPICKER_CHANGED, this, GetId(), m_path);
        HandleWindowEvent(event);
    }
}

void wxFileButton::SetPath(const wxString &str)
{
    m_path = str;

    if (m_dialog)
        UpdateDialogPath(m_dialog);
}

#endif      // wxUSE_FILEPICKERCTRL && defined(__WXGTK26__)




#if wxUSE_DIRPICKERCTRL && defined(__WXGTK26__)

#include <unistd.h> // chdir

//-----------------------------------------------------------------------------
// "current-folder-changed"
//-----------------------------------------------------------------------------

extern "C" {
static void gtk_dirbutton_currentfolderchanged_callback(GtkFileChooserButton *widget,
                                                        wxDirButton *p)
{
    // update the m_path member of the wxDirButtonGTK
    // unless the path was changed by wxDirButton::SetPath()
    if (p->m_bIgnoreNextChange)
    {
        p->m_bIgnoreNextChange=false;
        return;
    }
    wxASSERT(p);

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
        chdir(filename);

    // ...and fire an event
    wxFileDirPickerEvent event(wxEVT_COMMAND_DIRPICKER_CHANGED, p, p->GetId(), p->GetPath());
    p->HandleWindowEvent(event);
}
}


//-----------------------------------------------------------------------------
// wxDirButtonGTK
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxDirButton, wxButton)

bool wxDirButton::Create( wxWindow *parent, wxWindowID id,
                        const wxString &label, const wxString &path,
                        const wxString &message, const wxString &wildcard,
                        const wxPoint &pos, const wxSize &size,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    if ( !(style & wxDIRP_USE_TEXTCTRL) && !gtk_check_version(2,6,0) )
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
        SetPath(path);

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


        gtk_widget_show(m_widget);

        // GtkFileChooserButton signals
        g_signal_connect(m_widget, "current-folder-changed",
                         G_CALLBACK(gtk_dirbutton_currentfolderchanged_callback), this);

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
}

void wxDirButton::GTKUpdatePath(const char *gtkpath)
{
    m_path = wxString::FromUTF8(gtkpath);
}
void wxDirButton::SetPath(const wxString& str)
{
    if ( m_path == str )
    {
        // don't do anything and especially don't set m_bIgnoreNextChange
        return;
    }

    m_path = str;

    // wxDirButton uses the "current-folder-changed" signal which is triggered also
    // when we set the path on the dialog associated with this button; thus we need
    // to set the following flag to avoid sending a wxFileDirPickerEvent from this
    // function (which would be inconsistent with wxFileButton's behaviour and in
    // general with all wxWidgets control-manipulation functions which do not send events).
    m_bIgnoreNextChange = true;

    if (m_dialog)
        UpdateDialogPath(m_dialog);
}

#endif      // wxUSE_DIRPICKERCTRL && defined(__WXGTK26__)
