///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/filectrl.cpp
// Purpose:     wxGtkFileCtrl Implementation
// Author:      Diaa M. Sami
// Created:     2007-08-10
// RCS-ID:      $Id: filectrl.cpp 64429 2010-05-29 10:35:47Z VZ $
// Copyright:   (c) Diaa M. Sami
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "wx/filectrl.h"

#if wxUSE_FILECTRL && !defined(__WXUNIVERSAL__)

#ifndef WX_PRECOMP
#    include "wx/sizer.h"
#    include "wx/debug.h"
#endif

#include "wx/gtk/private.h"
#include "wx/filedlg.h"
#include "wx/filename.h"
#include "wx/scopeguard.h"
#include "wx/tokenzr.h"

//-----------------------------------------------------------------------------
// wxGtkFileChooser implementation
//-----------------------------------------------------------------------------

void wxGtkFileChooser::SetWidget(GtkFileChooser *w)
{
    // check arguments
    wxASSERT( w );
    wxASSERT( GTK_FILE_CHOOSER( w ) );

    this->m_widget = w;
}

wxString wxGtkFileChooser::GetPath() const
{
    wxGtkString str( gtk_file_chooser_get_filename( m_widget ) );

    wxString string;
    if (str.c_str() != NULL)
        string = wxString::FromUTF8(str);
    return string;
}

void wxGtkFileChooser::GetFilenames( wxArrayString& files ) const
{
    GetPaths( files );
    for ( size_t n = 0; n < files.GetCount(); ++n )
    {
        const wxFileName file( files[n] );
        files[n] = file.GetFullName();
    }
}

void wxGtkFileChooser::GetPaths( wxArrayString& paths ) const
{
    paths.Empty();
    if ( gtk_file_chooser_get_select_multiple( m_widget ) )
    {
        GSList *gpathsi = gtk_file_chooser_get_filenames( m_widget );
        GSList *gpaths = gpathsi;
        while ( gpathsi )
        {
            wxString file(wxString::FromUTF8(static_cast<gchar *>(gpathsi->data)));
            paths.Add( file );
            g_free( gpathsi->data );
            gpathsi = gpathsi->next;
        }

        g_slist_free( gpaths );
    }
    else
        paths.Add( GetPath() );
}

bool wxGtkFileChooser::SetPath( const wxString& path )
{
    if ( path.empty() )
        return true;

    return gtk_file_chooser_set_filename( m_widget, path.utf8_str() );
}

bool wxGtkFileChooser::SetDirectory( const wxString& dir )
{
    return gtk_file_chooser_set_current_folder( m_widget, dir.utf8_str() ) != 0;
}

wxString wxGtkFileChooser::GetDirectory() const
{
    const wxGtkString str( gtk_file_chooser_get_current_folder( m_widget ) );
    return wxString::FromUTF8(str);
}

wxString wxGtkFileChooser::GetFilename() const
{
    return wxFileName( GetPath() ).GetFullName();
}

void wxGtkFileChooser::SetWildcard( const wxString& wildCard )
{
    m_wildcards.Empty();

    // parse filters
    wxArrayString wildDescriptions, wildFilters;

    if ( !wxParseCommonDialogsFilter( wildCard, wildDescriptions, wildFilters ) )
    {
        wxFAIL_MSG( wxT( "wxGtkFileChooser::SetWildcard - bad wildcard string" ) );
    }
    else
    {
        // Parsing went fine. Set m_wildCard to be returned by wxGtkFileChooserBase::GetWildcard
        GtkFileChooser* chooser = m_widget;

        // empty current filter list:
        GSList* ifilters = gtk_file_chooser_list_filters( chooser );
        GSList* filters = ifilters;

        m_ignoreNextFilterEvent = true;
        wxON_BLOCK_EXIT_SET(m_ignoreNextFilterEvent, false);

        while ( ifilters )
        {
            gtk_file_chooser_remove_filter( chooser, GTK_FILE_FILTER( ifilters->data ) );
            ifilters = ifilters->next;
        }
        g_slist_free( filters );

        if (!wildCard.empty())
        {
            // add parsed to GtkChooser
            for ( size_t n = 0; n < wildFilters.GetCount(); ++n )
            {
                GtkFileFilter* filter = gtk_file_filter_new();

                gtk_file_filter_set_name( filter, wxGTK_CONV_SYS( wildDescriptions[n] ) );

                wxStringTokenizer exttok( wildFilters[n], wxT( ";" ) );

                int n1 = 1;
                while ( exttok.HasMoreTokens() )
                {
                    wxString token = exttok.GetNextToken();
                    gtk_file_filter_add_pattern( filter, wxGTK_CONV_SYS( token ) );

                    if (n1 == 1)
                        m_wildcards.Add( token ); // Only add first pattern to list, used later when saving
                    n1++;
                }

                gtk_file_chooser_add_filter( chooser, filter );
            }

            // Reset the filter index
            SetFilterIndex( 0 );
        }
    }
}

void wxGtkFileChooser::SetFilterIndex( int filterIndex )
{
    gpointer filter;
    GtkFileChooser *chooser = m_widget;
    GSList *filters = gtk_file_chooser_list_filters( chooser );

    filter = g_slist_nth_data( filters, filterIndex );

    if ( filter != NULL )
    {
        gtk_file_chooser_set_filter( chooser, GTK_FILE_FILTER( filter ) );
    }
    else
    {
        wxFAIL_MSG( wxT( "wxGtkFileChooser::SetFilterIndex - bad filter index" ) );
    }

    g_slist_free( filters );
}

int wxGtkFileChooser::GetFilterIndex() const
{
    GtkFileChooser *chooser = m_widget;
    GtkFileFilter *filter = gtk_file_chooser_get_filter( chooser );
    GSList *filters = gtk_file_chooser_list_filters( chooser );
    const gint index = g_slist_index( filters, filter );
    g_slist_free( filters );

    if ( index == -1 )
    {
        wxFAIL_MSG( wxT( "wxGtkFileChooser::GetFilterIndex - bad filter index returned by gtk+" ) );
        return 0;
    }
    else
        return index;
}

bool wxGtkFileChooser::HasFilterChoice() const
{
    return gtk_file_chooser_get_filter( m_widget ) != NULL;
}

//-----------------------------------------------------------------------------
// end wxGtkFileChooser Implementation
//-----------------------------------------------------------------------------

#if wxUSE_FILECTRL

// gtk signal handlers

extern "C"
{
    static void
    gtkfilechooserwidget_file_activated_callback( GtkWidget *WXUNUSED( widget ), wxGtkFileCtrl *fileCtrl )
    {
        GenerateFileActivatedEvent( fileCtrl, fileCtrl );
    }
}

extern "C"
{
    static void
    gtkfilechooserwidget_selection_changed_callback( GtkWidget *WXUNUSED( widget ), wxGtkFileCtrl *fileCtrl )
    {
        // check next selection event and ignore it if it has 0 files
        // because such events are redundantly generated by gtk.
        if ( fileCtrl->m_checkNextSelEvent )
        {
            wxArrayString filenames;
            fileCtrl->GetFilenames( filenames );

            if ( filenames.Count() != 0 )
                fileCtrl->m_checkNextSelEvent = false;
        }

        if ( !fileCtrl->m_checkNextSelEvent )
            GenerateSelectionChangedEvent( fileCtrl, fileCtrl );
    }
}

extern "C"
{
    static void
    gtkfilechooserwidget_folder_changed_callback( GtkWidget *WXUNUSED( widget ), wxGtkFileCtrl *fileCtrl )
    {
        if ( fileCtrl->m_ignoreNextFolderChangeEvent )
        {
            fileCtrl->m_ignoreNextFolderChangeEvent = false;
        }
        else
        {
            GenerateFolderChangedEvent( fileCtrl, fileCtrl );
        }

        fileCtrl->m_checkNextSelEvent = true;
    }
}

extern "C"
{
    static void
    gtkfilechooserwidget_notify_callback( GObject *WXUNUSED( gobject ), GParamSpec *arg1, wxGtkFileCtrl *fileCtrl )
    {
        const char *name = g_param_spec_get_name (arg1);
        if ( strcmp( name, "filter" ) == 0 &&
             fileCtrl->HasFilterChoice() &&
             !fileCtrl->GTKShouldIgnoreNextFilterEvent() )
        {
            GenerateFilterChangedEvent( fileCtrl, fileCtrl );
        }
    }
}

// wxGtkFileCtrl implementation

IMPLEMENT_DYNAMIC_CLASS( wxGtkFileCtrl, wxControl )

void wxGtkFileCtrl::Init()
{
    m_checkNextSelEvent = false;

    // ignore the first folder change event which is fired upon startup.
    m_ignoreNextFolderChangeEvent = true;
}

bool wxGtkFileCtrl::Create( wxWindow *parent,
                            wxWindowID id,
                            const wxString& defaultDirectory,
                            const wxString& defaultFileName,
                            const wxString& wildCard,
                            long style,
                            const wxPoint& pos,
                            const wxSize& size,
                            const wxString& name )
{
    if ( !PreCreation( parent, pos, size ) ||
            !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ) )
    {
        wxFAIL_MSG( wxT( "wxGtkFileCtrl creation failed" ) );
        return false;
    }

    GtkFileChooserAction gtkAction = GTK_FILE_CHOOSER_ACTION_OPEN;

    if ( style & wxFC_SAVE )
        gtkAction = GTK_FILE_CHOOSER_ACTION_SAVE;

    m_widget =  gtk_alignment_new ( 0, 0, 1, 1 );
    g_object_ref(m_widget);
    m_fcWidget = GTK_FILE_CHOOSER( gtk_file_chooser_widget_new(gtkAction) );
    gtk_widget_show ( GTK_WIDGET( m_fcWidget ) );
    gtk_container_add ( GTK_CONTAINER ( m_widget ), GTK_WIDGET( m_fcWidget ) );

    m_focusWidget = GTK_WIDGET( m_fcWidget );

    g_signal_connect ( m_fcWidget, "file-activated",
                       G_CALLBACK ( gtkfilechooserwidget_file_activated_callback ),
                       this );

    g_signal_connect ( m_fcWidget, "current-folder-changed",
                       G_CALLBACK ( gtkfilechooserwidget_folder_changed_callback ),
                       this );

    g_signal_connect ( m_fcWidget, "selection-changed",
                       G_CALLBACK ( gtkfilechooserwidget_selection_changed_callback ),
                       this );

    g_signal_connect ( m_fcWidget, "notify",
                       G_CALLBACK ( gtkfilechooserwidget_notify_callback ),
                       this );

    m_fc.SetWidget( m_fcWidget );

    if ( style & wxFC_MULTIPLE )
        gtk_file_chooser_set_select_multiple( m_fcWidget, true );

    SetWildcard( wildCard );

    // if defaultDir is specified it should contain the directory and
    // defaultFileName should contain the default name of the file, however if
    // directory is not given, defaultFileName contains both
    wxFileName fn;
    if ( defaultDirectory.empty() )
        fn.Assign( defaultFileName );
    else if ( !defaultFileName.empty() )
        fn.Assign( defaultDirectory, defaultFileName );
    else
        fn.AssignDir( defaultDirectory );

    // set the initial file name and/or directory
    const wxString dir = fn.GetPath();
    if ( !dir.empty() )
    {
        gtk_file_chooser_set_current_folder( m_fcWidget,
                                             dir.fn_str() );
    }

    const wxString fname = fn.GetFullName();
    if ( style & wxFC_SAVE )
    {
        if ( !fname.empty() )
        {
            gtk_file_chooser_set_current_name( m_fcWidget,
                                               fname.fn_str() );
        }
    }
    else // wxFC_OPEN
    {
        if ( !fname.empty() )
        {
            gtk_file_chooser_set_filename( m_fcWidget,
                                           fn.GetFullPath().fn_str() );
        }
    }

    m_parent->DoAddChild( this );

    PostCreation( size );

    return TRUE;
}

bool wxGtkFileCtrl::SetPath( const wxString& path )
{
    return m_fc.SetPath( path );
}

bool wxGtkFileCtrl::SetDirectory( const wxString& dir )
{
    return m_fc.SetDirectory( dir );
}

bool wxGtkFileCtrl::SetFilename( const wxString& name )
{
    if ( HasFlag( wxFC_SAVE ) )
    {
        gtk_file_chooser_set_current_name( m_fcWidget, wxGTK_CONV( name ) );
        return true;
    }
    else
        return SetPath( wxFileName( GetDirectory(), name ).GetFullPath() );
}

void wxGtkFileCtrl::SetWildcard( const wxString& wildCard )
{
    m_wildCard = wildCard;

    m_fc.SetWildcard( wildCard );
}

void wxGtkFileCtrl::SetFilterIndex( int filterIndex )
{
    m_fc.SetFilterIndex( filterIndex );
}

wxString wxGtkFileCtrl::GetPath() const
{
    return m_fc.GetPath();
}

void wxGtkFileCtrl::GetPaths( wxArrayString& paths ) const
{
    m_fc.GetPaths( paths );
}

wxString wxGtkFileCtrl::GetDirectory() const
{
    return m_fc.GetDirectory();
}

wxString wxGtkFileCtrl::GetFilename() const
{
    return m_fc.GetFilename();
}

void wxGtkFileCtrl::GetFilenames( wxArrayString& files ) const
{
    m_fc.GetFilenames( files );
}

void wxGtkFileCtrl::ShowHidden(bool show)
{
    // gtk_file_chooser_set_show_hidden() is new in 2.6
    g_object_set (G_OBJECT (m_fcWidget), "show-hidden", show, NULL);
}

#endif // wxUSE_FILECTRL

#endif // wxUSE_FILECTRL && !defined(__WXUNIVERSAL__)
