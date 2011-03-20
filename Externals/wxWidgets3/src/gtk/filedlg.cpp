/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/filedlg.cpp
// Purpose:     native implementation of wxFileDialog
// Author:      Robert Roebling, Zbigniew Zagorski, Mart Raudsepp
// Id:          $Id: filedlg.cpp 64381 2010-05-22 12:07:54Z VZ $
// Copyright:   (c) 1998 Robert Roebling, 2004 Zbigniew Zagorski, 2005 Mart Raudsepp
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FILEDLG

#include "wx/filedlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/msgdlg.h"
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

#include <unistd.h> // chdir

#include "wx/filename.h" // wxFilename
#include "wx/tokenzr.h" // wxStringTokenizer
#include "wx/filefn.h" // ::wxGetCwd

//-----------------------------------------------------------------------------
// "clicked" for OK-button
//-----------------------------------------------------------------------------

extern "C" {
static void gtk_filedialog_ok_callback(GtkWidget *widget, wxFileDialog *dialog)
{
    int style = dialog->GetWindowStyle();
    wxGtkString filename(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget)));

    // gtk version numbers must be identical with the one in ctor (that calls set_do_overwrite_confirmation)
#if GTK_CHECK_VERSION(2,7,3)
    if (gtk_check_version(2, 7, 3) != NULL)
#endif
    {
        if ((style & wxFD_SAVE) && (style & wxFD_OVERWRITE_PROMPT))
        {
            if ( g_file_test(filename, G_FILE_TEST_EXISTS) )
            {
                wxString msg;

                msg.Printf(
                    _("File '%s' already exists, do you really want to overwrite it?"),
                    wxString::FromUTF8(filename));

                wxMessageDialog dlg(dialog, msg, _("Confirm"),
                                   wxYES_NO | wxICON_QUESTION);
                if (dlg.ShowModal() != wxID_YES)
                    return;
            }
        }
    }

    if (style & wxFD_FILE_MUST_EXIST)
    {
        if ( !g_file_test(filename, G_FILE_TEST_EXISTS) )
        {
            wxMessageDialog dlg( dialog, _("Please choose an existing file."),
                                 _("Error"), wxOK| wxICON_ERROR);
            dlg.ShowModal();
            return;
        }
    }

    // change to the directory where the user went if asked
    if (style & wxFD_CHANGE_DIR)
    {
        // Use chdir to not care about filename encodings
        wxGtkString folder(g_path_get_dirname(filename));
        chdir(folder);
    }

    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, wxID_OK);
    event.SetEventObject(dialog);
    dialog->HandleWindowEvent(event);
}
}

//-----------------------------------------------------------------------------
// "clicked" for Cancel-button
//-----------------------------------------------------------------------------

extern "C"
{

static void
gtk_filedialog_cancel_callback(GtkWidget * WXUNUSED(w), wxFileDialog *dialog)
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, wxID_CANCEL);
    event.SetEventObject(dialog);
    dialog->HandleWindowEvent(event);
}

static void gtk_filedialog_response_callback(GtkWidget *w,
                                             gint response,
                                             wxFileDialog *dialog)
{
    if (response == GTK_RESPONSE_ACCEPT)
        gtk_filedialog_ok_callback(w, dialog);
    else    // GTK_RESPONSE_CANCEL or GTK_RESPONSE_NONE
        gtk_filedialog_cancel_callback(w, dialog);
}

static void gtk_filedialog_update_preview_callback(GtkFileChooser *chooser,
                                                   gpointer user_data)
{
    GtkWidget *preview = GTK_WIDGET(user_data);

    wxGtkString filename(gtk_file_chooser_get_preview_filename(chooser));

    if ( !filename )
        return;

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(filename, 128, 128, NULL);
    gboolean have_preview = pixbuf != NULL;

    gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);
    if ( pixbuf )
        g_object_unref (pixbuf);

    gtk_file_chooser_set_preview_widget_active(chooser, have_preview);
}

} // extern "C"

//-----------------------------------------------------------------------------
// "size_request" from m_extraControl
//-----------------------------------------------------------------------------

extern "C" {
static void extra_widget_size_request(GtkWidget*, GtkRequisition* req, wxWindow* win)
{
    // allow dialog to be resized smaller horizontally
    req->width = win->GetMinWidth();
}
}

void wxFileDialog::AddChildGTK(wxWindowGTK* child)
{
    g_signal_connect_after(child->m_widget, "size_request",
        G_CALLBACK(extra_widget_size_request), child);
    gtk_file_chooser_set_extra_widget(
        GTK_FILE_CHOOSER(m_widget), child->m_widget);
}

//-----------------------------------------------------------------------------
// wxFileDialog
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxFileDialog,wxFileDialogBase)

BEGIN_EVENT_TABLE(wxFileDialog,wxFileDialogBase)
    EVT_BUTTON(wxID_OK, wxFileDialog::OnFakeOk)
    EVT_SIZE(wxFileDialog::OnSize)
END_EVENT_TABLE()

wxFileDialog::wxFileDialog(wxWindow *parent, const wxString& message,
                           const wxString& defaultDir,
                           const wxString& defaultFileName,
                           const wxString& wildCard,
                           long style, const wxPoint& pos,
                           const wxSize& sz,
                           const wxString& name)
    : wxFileDialogBase()
{
    parent = GetParentForModalDialog(parent, style);

    if (!wxFileDialogBase::Create(parent, message, defaultDir, defaultFileName,
                                  wildCard, style, pos, sz, name))
    {
        return;
    }

    if (!PreCreation(parent, pos, wxDefaultSize) ||
        !CreateBase(parent, wxID_ANY, pos, wxDefaultSize, style,
                wxDefaultValidator, wxT("filedialog")))
    {
        wxFAIL_MSG( wxT("wxFileDialog creation failed") );
        return;
    }

    GtkFileChooserAction gtk_action;
    GtkWindow* gtk_parent = NULL;
    if (parent)
        gtk_parent = GTK_WINDOW( gtk_widget_get_toplevel(parent->m_widget) );

    const gchar* ok_btn_stock;
    if ( style & wxFD_SAVE )
    {
        gtk_action = GTK_FILE_CHOOSER_ACTION_SAVE;
        ok_btn_stock = GTK_STOCK_SAVE;
    }
    else
    {
        gtk_action = GTK_FILE_CHOOSER_ACTION_OPEN;
        ok_btn_stock = GTK_STOCK_OPEN;
    }

    m_widget = gtk_file_chooser_dialog_new(
                   wxGTK_CONV(m_message),
                   gtk_parent,
                   gtk_action,
                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                   ok_btn_stock, GTK_RESPONSE_ACCEPT,
                   NULL);
    g_object_ref(m_widget);
    GtkFileChooser* file_chooser = GTK_FILE_CHOOSER(m_widget);

    m_fc.SetWidget(file_chooser);

    gtk_dialog_set_default_response(GTK_DIALOG(m_widget), GTK_RESPONSE_ACCEPT);

    if ( style & wxFD_MULTIPLE )
        gtk_file_chooser_set_select_multiple(file_chooser, true);

    // gtk_widget_hide_on_delete is used here to avoid that Gtk automatically
    // destroys the dialog when the user press ESC on the dialog: in that case
    // a second call to ShowModal() would result in a bunch of Gtk-CRITICAL
    // errors...
    g_signal_connect(m_widget,
                    "delete_event",
                    G_CALLBACK (gtk_widget_hide_on_delete),
                    this);

    // local-only property could be set to false to allow non-local files to be
    // loaded. In that case get/set_uri(s) should be used instead of
    // get/set_filename(s) everywhere and the GtkFileChooserDialog should
    // probably also be created with a backend, e.g "gnome-vfs", "default", ...
    // (gtk_file_chooser_dialog_new_with_backend). Currently local-only is kept
    // as the default - true:
    // gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(m_widget), true);

    g_signal_connect (m_widget, "response",
        G_CALLBACK (gtk_filedialog_response_callback), this);


    // deal with extensions/filters
    SetWildcard(wildCard);

    wxString defaultFileNameWithExt = defaultFileName;
    if ( !wildCard.empty() && !defaultFileName.empty() &&
            !wxFileName(defaultFileName).HasExt() )
    {
        // append the default extension to the initial file name: GTK won't do
        // it for us by default (unlike e.g. MSW)
        const wxString defaultExt = m_fc.GetCurrentWildCard().AfterFirst('.');
        if ( defaultExt.find_first_of("?*") == wxString::npos )
            defaultFileNameWithExt += "." + defaultExt;
    }


    // if defaultDir is specified it should contain the directory and
    // defaultFileName should contain the default name of the file, however if
    // directory is not given, defaultFileName contains both
    wxFileName fn;
    if ( defaultDir.empty() )
        fn.Assign(defaultFileNameWithExt);
    else if ( !defaultFileNameWithExt.empty() )
        fn.Assign(defaultDir, defaultFileNameWithExt);
    else
        fn.AssignDir(defaultDir);

    // set the initial file name and/or directory
    fn.MakeAbsolute(); // GTK+ needs absolute path
    const wxString dir = fn.GetPath();
    if ( !dir.empty() )
    {
        gtk_file_chooser_set_current_folder(file_chooser, dir.fn_str());
    }

    const wxString fname = fn.GetFullName();
    if ( style & wxFD_SAVE )
    {
        if ( !fname.empty() )
        {
            gtk_file_chooser_set_current_name(file_chooser, fname.fn_str());
        }

#if GTK_CHECK_VERSION(2,7,3)
        if ((style & wxFD_OVERWRITE_PROMPT) && !gtk_check_version(2,7,3))
            gtk_file_chooser_set_do_overwrite_confirmation(file_chooser, true);
#endif
    }
    else // wxFD_OPEN
    {
        if ( !fname.empty() )
        {
            gtk_file_chooser_set_filename(file_chooser,
                                          fn.GetFullPath().fn_str());
        }
    }

    if ( style & wxFD_PREVIEW )
    {
        GtkWidget *previewImage = gtk_image_new();

        gtk_file_chooser_set_preview_widget(file_chooser, previewImage);
        g_signal_connect(m_widget, "update-preview",
                         G_CALLBACK(gtk_filedialog_update_preview_callback),
                         previewImage);
    }
}

wxFileDialog::~wxFileDialog()
{
    if (m_extraControl)
    {
        // get chooser to drop its reference right now, allowing wxWindow dtor
        // to verify that ref count drops to zero
        gtk_file_chooser_set_extra_widget(
            GTK_FILE_CHOOSER(m_widget), NULL);
    }
}

void wxFileDialog::OnFakeOk(wxCommandEvent& WXUNUSED(event))
{
    EndDialog(wxID_OK);
}

int wxFileDialog::ShowModal()
{
    CreateExtraControl();

    return wxDialog::ShowModal();
}

void wxFileDialog::DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                             int WXUNUSED(width), int WXUNUSED(height),
                             int WXUNUSED(sizeFlags))
{
}

void wxFileDialog::OnSize(wxSizeEvent&)
{
    // avoid calling DoLayout(), which will set the (wrong) size of
    // m_extraControl, its size is managed by GtkFileChooser
}

wxString wxFileDialog::GetPath() const
{
    return m_fc.GetPath();
}

void wxFileDialog::GetFilenames(wxArrayString& files) const
{
    m_fc.GetFilenames( files );
}

void wxFileDialog::GetPaths(wxArrayString& paths) const
{
    m_fc.GetPaths( paths );
}

void wxFileDialog::SetMessage(const wxString& message)
{
    m_message = message;
    SetTitle(message);
}

void wxFileDialog::SetPath(const wxString& path)
{
    // we need an absolute path for GTK native chooser so ensure that we have
    // it
    wxFileName fn(path);
    fn.MakeAbsolute();
    m_fc.SetPath(fn.GetFullPath());
}

void wxFileDialog::SetDirectory(const wxString& dir)
{
    if (m_fc.SetDirectory( dir ))
    {
        // Cache the dir, as gtk_file_chooser_get_current_folder()
        // doesn't return anything until the dialog has been shown
        m_dir = dir;
    }
}

wxString wxFileDialog::GetDirectory() const
{
    wxString currentDir( m_fc.GetDirectory() );
    if (currentDir.empty())
    {
        // m_fc.GetDirectory() will return empty until the dialog has been shown
        // in which case use any previously provided value
        currentDir = m_dir;
    }
    return currentDir;
}

void wxFileDialog::SetFilename(const wxString& name)
{
    if (HasFdFlag(wxFD_SAVE))
    {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(m_widget), wxGTK_CONV(name));
        m_fileName = name;
    }

    else
    {
        wxString path( GetDirectory() );
        if (path.empty())
        {
            // SetPath() fires an assert if fed other than filepaths
            return;
        }
        SetPath(wxFileName(path, name).GetFullPath());
        m_fileName = name;
    }
}

wxString wxFileDialog::GetFilename() const
{
    wxString currentFilename( m_fc.GetFilename() );
    if (currentFilename.empty())
    {
        // m_fc.GetFilename() will return empty until the dialog has been shown
        // in which case use any previously provided value
        currentFilename = m_fileName;
    }
    return currentFilename;
}

void wxFileDialog::SetWildcard(const wxString& wildCard)
{
    wxFileDialogBase::SetWildcard(wildCard);
    m_fc.SetWildcard( GetWildcard() );
}

void wxFileDialog::SetFilterIndex(int filterIndex)
{
    m_fc.SetFilterIndex( filterIndex);
}

int wxFileDialog::GetFilterIndex() const
{
    return m_fc.GetFilterIndex();
}

#endif // wxUSE_FILEDLG
