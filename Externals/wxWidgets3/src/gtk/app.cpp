/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/app.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/app.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/memory.h"
    #include "wx/font.h"
#endif

#include "wx/thread.h"

#ifdef __WXGPE__
    #include <gpe/init.h>
#endif

#include "wx/apptrait.h"
#include "wx/fontmap.h"

#if wxUSE_LIBHILDON
    #include <hildon-widgets/hildon-program.h>
#endif // wxUSE_LIBHILDON

#if wxUSE_LIBHILDON2
    #include <hildon/hildon.h>
#endif // wxUSE_LIBHILDON2

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// link GnomeVFS
//-----------------------------------------------------------------------------

#if wxUSE_MIMETYPE && wxUSE_LIBGNOMEVFS
    #include "wx/link.h"
    wxFORCE_LINK_MODULE(gnome_vfs)
#endif

//-----------------------------------------------------------------------------
// local functions
//-----------------------------------------------------------------------------

// One-shot signal emission hook, to install idle handler.
extern "C" {
static gboolean
wx_emission_hook(GSignalInvocationHint*, guint, const GValue*, gpointer data)
{
    wxApp* app = wxTheApp;
    if (app != NULL)
        app->WakeUpIdle();
    bool* hook_installed = (bool*)data;
    // record that hook is not installed
    *hook_installed = false;
    // remove hook
    return false;
}
}

// Add signal emission hooks, to re-install idle handler when needed.
static void wx_add_idle_hooks()
{
    // "event" hook
    {
        static bool hook_installed;
        if (!hook_installed)
        {
            static guint sig_id;
            if (sig_id == 0)
                sig_id = g_signal_lookup("event", GTK_TYPE_WIDGET);
            hook_installed = true;
            g_signal_add_emission_hook(
                sig_id, 0, wx_emission_hook, &hook_installed, NULL);
        }
    }
    // "size_allocate" hook
    // Needed to match the behaviour of the old idle system,
    // but probably not necessary.
    {
        static bool hook_installed;
        if (!hook_installed)
        {
            static guint sig_id;
            if (sig_id == 0)
                sig_id = g_signal_lookup("size_allocate", GTK_TYPE_WIDGET);
            hook_installed = true;
            g_signal_add_emission_hook(
                sig_id, 0, wx_emission_hook, &hook_installed, NULL);
        }
    }
}

extern "C" {
static gboolean wxapp_idle_callback(gpointer)
{
    return wxTheApp->DoIdle();
}
}

bool wxApp::DoIdle()
{
    guint id_save;
    {
        // Allow another idle source to be added while this one is busy.
        // Needed if an idle event handler runs a new event loop,
        // for example by showing a dialog.
#if wxUSE_THREADS
        wxMutexLocker lock(m_idleMutex);
#endif
        id_save = m_idleSourceId;
        m_idleSourceId = 0;
        wx_add_idle_hooks();

#if wxDEBUG_LEVEL
        // don't generate the idle events while the assert modal dialog is shown,
        // this matches the behaviour of wxMSW
        if (m_isInAssert)
            return false;
#endif
    }

    gdk_threads_enter();
    bool needMore;
    do {
        ProcessPendingEvents();

        needMore = ProcessIdle();
    } while (needMore && gtk_events_pending() == 0);
    gdk_threads_leave();

#if wxUSE_THREADS
    wxMutexLocker lock(m_idleMutex);
#endif

    bool keepSource = false;
    // if a new idle source has not been added, either as a result of idle
    // processing above or by another thread calling WakeUpIdle()
    if (m_idleSourceId == 0)
    {
        // if more idle processing was requested or pending events have appeared
        if (needMore || HasPendingEvents())
        {
            // keep this source installed
            m_idleSourceId = id_save;
            keepSource = true;
        }
        else // add hooks and remove this source
            wx_add_idle_hooks();
    }
    // else remove this source, leave new one installed
    // we must keep an idle source, otherwise a wakeup could be lost

    return keepSource;
}

//-----------------------------------------------------------------------------
// Access to the root window global
//-----------------------------------------------------------------------------

GtkWidget* wxGetRootWindow()
{
    static GtkWidget *s_RootWindow = NULL;

    if (s_RootWindow == NULL)
    {
        s_RootWindow = gtk_window_new( GTK_WINDOW_TOPLEVEL );
        gtk_widget_realize( s_RootWindow );
    }
    return s_RootWindow;
}

//-----------------------------------------------------------------------------
// wxApp
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxApp,wxEvtHandler)

wxApp::wxApp()
{
    m_isInAssert = false;
    m_idleSourceId = 0;
}

wxApp::~wxApp()
{
}

bool wxApp::SetNativeTheme(const wxString& theme)
{
#ifdef __WXGTK3__
    wxUnusedVar(theme);
    return false;
#else
    wxString path;
    path = gtk_rc_get_theme_dir();
    path += "/";
    path += theme.utf8_str();
    path += "/gtk-2.0/gtkrc";

    if ( wxFileExists(path.utf8_str()) )
        gtk_rc_add_default_file(path.utf8_str());
    else if ( wxFileExists(theme.utf8_str()) )
        gtk_rc_add_default_file(theme.utf8_str());
    else
    {
        wxLogWarning("Theme \"%s\" not available.", theme);

        return false;
    }

    gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);

    return true;
#endif
}

bool wxApp::OnInitGui()
{
    if ( !wxAppBase::OnInitGui() )
        return false;

#ifndef __WXGTK3__
    // if this is a wxGLApp (derived from wxApp), and we've already
    // chosen a specific visual, then derive the GdkVisual from that
    if ( GetXVisualInfo() )
    {
        GdkVisual* vis = gtk_widget_get_default_visual();

        GdkColormap *colormap = gdk_colormap_new( vis, FALSE );
        gtk_widget_set_default_colormap( colormap );
    }
    else
    {
        // On some machines, the default visual is just 256 colours, so
        // we make sure we get the best. This can sometimes be wasteful.
        if (m_useBestVisual)
        {
            if (m_forceTrueColour)
            {
                GdkVisual* visual = gdk_visual_get_best_with_both( 24, GDK_VISUAL_TRUE_COLOR );
                if (!visual)
                {
                    wxLogError(wxT("Unable to initialize TrueColor visual."));
                    return false;
                }
                GdkColormap *colormap = gdk_colormap_new( visual, FALSE );
                gtk_widget_set_default_colormap( colormap );
            }
            else
            {
                if (gdk_visual_get_best() != gdk_visual_get_system())
                {
                    GdkVisual* visual = gdk_visual_get_best();
                    GdkColormap *colormap = gdk_colormap_new( visual, FALSE );
                    gtk_widget_set_default_colormap( colormap );
                }
            }
        }
    }
#endif

#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
    if ( !GetHildonProgram() )
    {
        wxLogError(_("Unable to initialize Hildon program"));
        return false;
    }
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2

    return true;
}

// use unusual names for the parameters to avoid conflict with wxApp::arg[cv]
bool wxApp::Initialize(int& argc_, wxChar **argv_)
{
    if ( !wxAppBase::Initialize(argc_, argv_) )
        return false;

#if wxUSE_THREADS
    if (!g_thread_supported())
    {
        g_thread_init(NULL);
        gdk_threads_init();
    }
#endif // wxUSE_THREADS

    // gtk+ 2.0 supports Unicode through UTF-8 strings
    wxConvCurrent = &wxConvUTF8;

#ifdef __UNIX__
    // decide which conversion to use for the file names

    // (1) this variable exists for the sole purpose of specifying the encoding
    //     of the filenames for GTK+ programs, so use it if it is set
    wxString encName(wxGetenv(wxT("G_FILENAME_ENCODING")));
    encName = encName.BeforeFirst(wxT(','));
    if (encName.CmpNoCase(wxT("@locale")) == 0)
        encName.clear();
    encName.MakeUpper();
    if (encName.empty())
    {
#if wxUSE_INTL
        // (2) if a non default locale is set, assume that the user wants his
        //     filenames in this locale too
        encName = wxLocale::GetSystemEncodingName().Upper();

        // But don't consider ASCII in this case.
        if ( !encName.empty() )
        {
#if wxUSE_FONTMAP
            wxFontEncoding enc = wxFontMapperBase::GetEncodingFromName(encName);
            if ( enc == wxFONTENCODING_DEFAULT )
#else // !wxUSE_FONTMAP
            if ( encName == wxT("US-ASCII") )
#endif // wxUSE_FONTMAP/!wxUSE_FONTMAP
            {
                // This means US-ASCII when returned from GetEncodingFromName().
                encName.clear();
            }
        }
#endif // wxUSE_INTL

        // (3) finally use UTF-8 by default
        if ( encName.empty() )
            encName = wxT("UTF-8");
        wxSetEnv(wxT("G_FILENAME_ENCODING"), encName);
    }

    static wxConvBrokenFileNames fileconv(encName);
    wxConvFileName = &fileconv;
#endif // __UNIX__


    bool init_result;
    int i;

#if wxUSE_UNICODE
    // gtk_init() wants UTF-8, not wchar_t, so convert
    char **argvGTK = new char *[argc_ + 1];
    for ( i = 0; i < argc_; i++ )
    {
        argvGTK[i] = wxStrdupA(wxConvUTF8.cWX2MB(argv_[i]));
    }

    argvGTK[argc_] = NULL;

    int argcGTK = argc_;

    // Prevent gtk_init_check() from changing the locale automatically for
    // consistency with the other ports that don't do it. If necessary,
    // wxApp::SetCLocale() may be explicitly called.
    gtk_disable_setlocale();

#ifdef __WXGPE__
    init_result = true;  // is there a _check() version of this?
    gpe_application_init( &argcGTK, &argvGTK );
#else
    init_result = gtk_init_check( &argcGTK, &argvGTK ) != 0;
#endif

    if ( argcGTK != argc_ )
    {
        // we have to drop the parameters which were consumed by GTK+
        for ( i = 0; i < argcGTK; i++ )
        {
            while ( strcmp(wxConvUTF8.cWX2MB(argv_[i]), argvGTK[i]) != 0 )
            {
                memmove(argv_ + i, argv_ + i + 1, (argc_ - i)*sizeof(*argv_));
            }
        }

        argc_ = argcGTK;
        argv_[argc_] = NULL;
    }
    //else: gtk_init() didn't modify our parameters

    // free our copy
    for ( i = 0; i < argcGTK; i++ )
    {
        free(argvGTK[i]);
    }

    delete [] argvGTK;
#else // !wxUSE_UNICODE
    // gtk_init() shouldn't actually change argv_ itself (just its contents) so
    // it's ok to pass pointer to it
    init_result = gtk_init_check( &argc_, &argv_ );
#endif // wxUSE_UNICODE/!wxUSE_UNICODE

    // update internal arg[cv] as GTK+ may have removed processed options:
    this->argc = argc_;
    this->argv = argv_;

    if ( m_traits )
    {
        // if there are still GTK+ standard options unparsed in the command
        // line, it means that they were not syntactically correct and GTK+
        // already printed a warning on the command line and we should now
        // exit:
        wxArrayString opt, desc;
        m_traits->GetStandardCmdLineOptions(opt, desc);

        for ( i = 0; i < argc_; i++ )
        {
            // leave just the names of the options with values
            const wxString str = wxString(argv_[i]).BeforeFirst('=');

            for ( size_t j = 0; j < opt.size(); j++ )
            {
                // remove the leading spaces from the option string as it does
                // have them
                if ( opt[j].Trim(false).BeforeFirst('=') == str )
                {
                    // a GTK+ option can be left on the command line only if
                    // there was an error in (or before, in another standard
                    // options) it, so abort, just as we do if incorrect
                    // program option is given
                    wxLogError(_("Invalid GTK+ command line option, use \"%s --help\""),
                               argv_[0]);
                    return false;
                }
            }
        }
    }

    if ( !init_result )
    {
        wxLogError(_("Unable to initialize GTK+, is DISPLAY set properly?"));
        return false;
    }

    // we cannot enter threads before gtk_init is done
    gdk_threads_enter();

#if wxUSE_INTL
    wxFont::SetDefaultEncoding(wxLocale::GetSystemEncoding());
#endif

    // make sure GtkWidget type is loaded, idle hooks need it
    g_type_class_ref(GTK_TYPE_WIDGET);
    WakeUpIdle();

    return true;
}

void wxApp::CleanUp()
{
    if (m_idleSourceId != 0)
        g_source_remove(m_idleSourceId);

    // release reference acquired by Initialize()
    g_type_class_unref(g_type_class_peek(GTK_TYPE_WIDGET));

    gdk_threads_leave();

    wxAppBase::CleanUp();
}

void wxApp::WakeUpIdle()
{
#if wxUSE_THREADS
    wxMutexLocker lock(m_idleMutex);
#endif
    if (m_idleSourceId == 0)
        m_idleSourceId = g_idle_add_full(G_PRIORITY_LOW, wxapp_idle_callback, NULL, NULL);
}

// Checking for pending events requires first removing our idle source,
// otherwise it will cause the check to always return true.
bool wxApp::EventsPending()
{
#if wxUSE_THREADS
    wxMutexLocker lock(m_idleMutex);
#endif
    if (m_idleSourceId != 0)
    {
        g_source_remove(m_idleSourceId);
        m_idleSourceId = 0;
        wx_add_idle_hooks();
    }
    return gtk_events_pending() != 0;
}

void wxApp::OnAssertFailure(const wxChar *file,
                            int line,
                            const wxChar* func,
                            const wxChar* cond,
                            const wxChar *msg)
{
    // there is no need to do anything if asserts are disabled in this build
    // anyhow
#if wxDEBUG_LEVEL
    // block wx idle events while assert dialog is showing
    m_isInAssert = true;

    wxAppBase::OnAssertFailure(file, line, func, cond, msg);

    m_isInAssert = false;
#else // !wxDEBUG_LEVEL
    wxUnusedVar(file);
    wxUnusedVar(line);
    wxUnusedVar(func);
    wxUnusedVar(cond);
    wxUnusedVar(msg);
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL
}

#if wxUSE_THREADS
void wxGUIAppTraits::MutexGuiEnter()
{
    gdk_threads_enter();
}

void wxGUIAppTraits::MutexGuiLeave()
{
    gdk_threads_leave();
}
#endif // wxUSE_THREADS

/* static */
bool wxApp::GTKIsUsingGlobalMenu()
{
    static int s_isUsingGlobalMenu = -1;
    if ( s_isUsingGlobalMenu == -1 )
    {
        // Currently we just check for this environment variable because this
        // is how support for the global menu is implemented under Ubuntu.
        //
        // If we ever get false positives, we could also check for
        // XDG_CURRENT_DESKTOP env var being set to "Unity".
        wxString proxy;
        s_isUsingGlobalMenu = wxGetEnv("UBUNTU_MENUPROXY", &proxy) &&
                                !proxy.empty() && proxy != "0";
    }

    return s_isUsingGlobalMenu == 1;
}

#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
// Maemo-specific method: get the main program object
HildonProgram *wxApp::GetHildonProgram()
{
    return hildon_program_get_instance();
}

#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2
