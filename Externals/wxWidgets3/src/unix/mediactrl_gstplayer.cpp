/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/mediactrl.cpp
// Purpose:     GStreamer backend for Unix
// Author:      Sebastian Dröge <sebastian@centricular.com>
// Created:     2016-02-28
// Copyright:   (c) 2004-2005 Ryan Norton
//              (c) 2016 Sebastian Dröge <sebastian@centricular.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_MEDIACTRL && wxUSE_GSTREAMER && wxUSE_GSTREAMER_PLAYER

#include "wx/mediactrl.h"

#include <gst/player/player.h>      // main gstreamer player header

#ifndef  WX_PRECOMP
    #include "wx/log.h"             // wxLogDebug/wxLogSysError/wxLogTrace
    #include "wx/app.h"             // wxTheApp->argc, wxTheApp->argv
    #include "wx/timer.h"           // wxTimer
#endif

#include "wx/filesys.h"             // FileNameToURL()
#include "wx/thread.h"              // wxMutex/wxMutexLocker
#include "wx/vector.h"              // wxVector<wxString>

#ifdef __WXGTK__
    #include <gtk/gtk.h>
    #include <gdk/gdkx.h>
    #include "wx/gtk/private/gtk2-compat.h"
#endif

//=============================================================================
//  Declarations
//=============================================================================
//-----------------------------------------------------------------------------
//  wxLogTrace mask string
//-----------------------------------------------------------------------------
#define wxTRACE_GStreamer wxT("GStreamer")

//-----------------------------------------------------------------------------
//
//  wxGStreamerMediaBackend
//
//-----------------------------------------------------------------------------
class WXDLLIMPEXP_MEDIA
    wxGStreamerMediaBackend : public wxMediaBackendCommonBase
{
public:

    wxGStreamerMediaBackend();
    virtual ~wxGStreamerMediaBackend();

    virtual bool CreateControl(wxControl* ctrl, wxWindow* parent,
                                     wxWindowID id,
                                     const wxPoint& pos,
                                     const wxSize& size,
                                     long style,
                                     const wxValidator& validator,
                                     const wxString& name) wxOVERRIDE;

    virtual bool Play() wxOVERRIDE;
    virtual bool Pause() wxOVERRIDE;
    virtual bool Stop() wxOVERRIDE;

    virtual bool Load(const wxString& fileName) wxOVERRIDE;
    virtual bool Load(const wxURI& location) wxOVERRIDE;
    virtual bool Load(const wxURI& location,
                      const wxURI& proxy) wxOVERRIDE
        { return wxMediaBackendCommonBase::Load(location, proxy); }


    virtual bool SetPosition(wxLongLong where) wxOVERRIDE;
    virtual wxLongLong GetPosition() wxOVERRIDE;
    virtual wxLongLong GetDuration() wxOVERRIDE;

    virtual void Move(int x, int y, int w, int h) wxOVERRIDE;
    wxSize GetVideoSize() const wxOVERRIDE;

    virtual double GetPlaybackRate() wxOVERRIDE;
    virtual bool SetPlaybackRate(double dRate) wxOVERRIDE;

    virtual wxMediaState GetState() wxOVERRIDE;

    virtual bool SetVolume(double dVolume) wxOVERRIDE;
    virtual double GetVolume() wxOVERRIDE;

    virtual wxLongLong GetDownloadProgress() wxOVERRIDE;
    virtual wxLongLong GetDownloadTotal() wxOVERRIDE;

    bool DoLoad(const wxString& locstring);
    wxMediaCtrl* GetControl() { return m_ctrl; } // for C Callbacks

    void VideoDimensionsChanged(int width, int height);
    void StateChanged(GstPlayerState state);
    void EndOfStream();

    GstPlayer              *m_player;
    GstPlayerVideoRenderer *m_video_renderer;
    wxSize                  m_videoSize;
    wxMediaState            m_last_state;
    bool                    m_loaded;

    wxDECLARE_DYNAMIC_CLASS(wxGStreamerMediaBackend);
};

//=============================================================================
// Implementation
//=============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxGStreamerMediaBackend, wxMediaBackend);

//-----------------------------------------------------------------------------
//
// C Callbacks
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// "expose_event" from m_ctrl->m_wxwindow
//
// Handle GTK expose event from our window - here we hopefully
// redraw the video in the case of pausing and other instances...
// (Returns TRUE to pass to other handlers, FALSE if not)
//
//-----------------------------------------------------------------------------
#ifdef __WXGTK__
static gboolean
#ifdef __WXGTK3__
draw_callback(GtkWidget* widget, cairo_t* cr, wxGStreamerMediaBackend* be)
#else
expose_event_callback(GtkWidget* widget, GdkEventExpose* event, wxGStreamerMediaBackend* be)
#endif
{
    // If we have actual video.....
    if(!(be->m_videoSize.x==0&&be->m_videoSize.y==0))
    {
        // GST Doesn't redraw automatically while paused
        // Plus, the video sometimes doesn't redraw when it looses focus
        // or is painted over so we just tell it to redraw...
        gst_player_video_overlay_video_renderer_expose(GST_PLAYER_VIDEO_OVERLAY_VIDEO_RENDERER(be->m_video_renderer));
    }
    else
    {
        // draw a black background like some other backends do....
#ifdef __WXGTK3__
        GtkAllocation a;
        gtk_widget_get_allocation(widget, &a);
        cairo_rectangle(cr, 0, 0, a.width, a.height);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_fill(cr);
#else
        gdk_draw_rectangle (event->window, widget->style->black_gc, TRUE, 0, 0,
                            widget->allocation.width,
                            widget->allocation.height);
#endif
    }

    return FALSE;
}
#endif // wxGTK

//-----------------------------------------------------------------------------
// "realize" from m_ctrl->m_wxwindow
//
// If the window wasn't realized when Load was called, this is the
// callback for when it is - the purpose of which is to tell
// GStreamer to play the video in our control
//-----------------------------------------------------------------------------
#ifdef __WXGTK__
extern "C" {
static void realize_callback(GtkWidget* widget, wxGStreamerMediaBackend* be)
{
    gdk_flush();

    GdkWindow* window = gtk_widget_get_window(widget);
    wxASSERT(window);

    gst_player_video_overlay_video_renderer_set_window_handle(GST_PLAYER_VIDEO_OVERLAY_VIDEO_RENDERER(be->m_video_renderer),
                                (gpointer) GDK_WINDOW_XID(window)
                                );
    GtkWidget* w = be->GetControl()->m_wxwindow;
#ifdef __WXGTK3__
    g_signal_connect(w, "draw", G_CALLBACK(draw_callback), be);
#else
    g_signal_connect(w, "expose_event", G_CALLBACK(expose_event_callback), be);
#endif
}
}
#endif // wxGTK

wxGStreamerMediaBackend::wxGStreamerMediaBackend()
  : m_player(0), m_video_renderer(0), m_videoSize(0, 0), m_last_state(wxMEDIASTATE_STOPPED), m_loaded(false)
{

}

wxGStreamerMediaBackend::~wxGStreamerMediaBackend()
{
    m_video_renderer = NULL;
    if (m_player)
        gst_object_unref(m_player);
    m_player = NULL;
}

extern "C" {
static void video_dimensions_changed_callback(GstPlayer * WXUNUSED(player), gint width, gint height, wxGStreamerMediaBackend* be)
{
    be->VideoDimensionsChanged(width, height);
}

static void state_changed_callback(GstPlayer * WXUNUSED(player), GstPlayerState state, wxGStreamerMediaBackend* be)
{
    be->StateChanged(state);
}

static void end_of_stream_callback(GstPlayer * WXUNUSED(player), wxGStreamerMediaBackend* be)
{
    be->EndOfStream();
}
}

bool wxGStreamerMediaBackend::CreateControl(wxControl* ctrl, wxWindow* parent,
                                            wxWindowID id,
                                            const wxPoint& pos,
                                            const wxSize& size,
                                            long style,
                                            const wxValidator& validator,
                                            const wxString& name)
{
    //
    //init gstreamer
    //

    //Convert arguments to unicode if enabled
#if wxUSE_UNICODE
    int i;
    char **argvGST = new char*[wxTheApp->argc + 1];
    for ( i = 0; i < wxTheApp->argc; i++ )
    {
        argvGST[i] = wxStrdupA(wxTheApp->argv[i].utf8_str());
    }

    argvGST[wxTheApp->argc] = NULL;

    int argcGST = wxTheApp->argc;
#else
#define argcGST wxTheApp->argc
#define argvGST wxTheApp->argv
#endif

    //Really init gstreamer
    gboolean bInited;
    GError* error = NULL;
    bInited = gst_init_check(&argcGST, &argvGST, &error);

    // Cleanup arguments for unicode case
#if wxUSE_UNICODE
    for ( i = 0; i < argcGST; i++ )
    {
        free(argvGST[i]);
    }

    delete [] argvGST;
#endif

    if(!bInited)    //gst_init_check fail?
    {
        if(error)
        {
            wxLogSysError(wxT("Could not initialize GStreamer\n")
                          wxT("Error Message:%s"),
                          (const wxChar*) wxConvUTF8.cMB2WX(error->message)
                         );
            g_error_free(error);
        }
        else
            wxLogSysError(wxT("Could not initialize GStreamer"));

        return false;
    }

    //
    // wxControl creation
    //
    m_ctrl = wxStaticCast(ctrl, wxMediaCtrl);

#ifdef __WXGTK__
    // We handle our own GTK expose events
    m_ctrl->m_noExpose = true;
#endif

    if( !m_ctrl->wxControl::Create(parent, id, pos, size,
                            style,  // TODO: remove borders???
                            validator, name) )
    {
        wxFAIL_MSG(wxT("Could not create wxControl!!!"));
        return false;
    }

#ifdef __WXGTK__
    // Turn off double-buffering so that
    // so it doesn't draw over the video and cause sporadic
    // disappearances of the video
    gtk_widget_set_double_buffered(m_ctrl->m_wxwindow, FALSE);
#endif

    // don't erase the background of our control window
    // so that resizing is a bit smoother
    m_ctrl->SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    // Tell gstreamer to play in our window
    gpointer window_handle = NULL;
#ifdef __WXGTK__
    if (!gtk_widget_get_realized(m_ctrl->m_wxwindow))
    {
        // Not realized yet - set to connect at realization time
        g_signal_connect (m_ctrl->m_wxwindow,
                          "realize",
                          G_CALLBACK (realize_callback),
                          this);
    }
    else
    {
        GdkWindow* window = gtk_widget_get_window(m_ctrl->m_wxwindow);
        wxASSERT(window);
        window_handle = (gpointer) GDK_WINDOW_XID(window);

        GtkWidget* w = m_ctrl->m_wxwindow;
#ifdef __WXGTK3__
        g_signal_connect(w, "draw", G_CALLBACK(draw_callback), this);
#else
        g_signal_connect(w, "expose_event", G_CALLBACK(expose_event_callback), this);
#endif
    }
#else
    window_handle = ctrl->GetHandle();
#endif

    m_video_renderer = gst_player_video_overlay_video_renderer_new(window_handle);
    m_player = gst_player_new(m_video_renderer, gst_player_g_main_context_signal_dispatcher_new(NULL));

    g_signal_connect(m_player, "video-dimensions-changed", G_CALLBACK(video_dimensions_changed_callback), this);
    g_signal_connect(m_player, "state-changed", G_CALLBACK(state_changed_callback), this);
    g_signal_connect(m_player, "end-of-stream", G_CALLBACK(end_of_stream_callback), this);

    return true;
}

bool wxGStreamerMediaBackend::Play()
{
    gst_player_play(m_player);

    return true;
}

bool wxGStreamerMediaBackend::Pause()
{
    gst_player_pause(m_player);

    return true;
}

bool wxGStreamerMediaBackend::Stop()
{
    gst_player_stop(m_player);

    return true;
}

bool wxGStreamerMediaBackend::Load(const wxString& fileName)
{
    return DoLoad(wxFileSystem::FileNameToURL(fileName));
}

bool wxGStreamerMediaBackend::Load(const wxURI& location)
{
    return DoLoad(location.BuildURI());
}

bool wxGStreamerMediaBackend::DoLoad(const wxString& locstring)
{
    // Make sure the passed URI is valid and tell playbin to load it
    // non-file uris are encoded
    wxASSERT(gst_uri_protocol_is_valid("file"));
    wxASSERT(gst_uri_is_valid(locstring.mb_str()));

    gst_player_stop(m_player);
    m_loaded = false;
    gst_player_set_uri(m_player, (const char*)locstring.mb_str());
    gst_player_pause(m_player);

    return true;
}

void wxGStreamerMediaBackend::VideoDimensionsChanged(int width, int height)
{
    if (m_loaded) {
        m_videoSize.x = width;
        m_videoSize.y = height;
        NotifyMovieSizeChanged();
    }
}

void wxGStreamerMediaBackend::StateChanged(GstPlayerState state)
{
  switch (state) {
    case GST_PLAYER_STATE_BUFFERING:
    case GST_PLAYER_STATE_PAUSED:
      if (!m_loaded) {
          NotifyMovieLoaded();
          m_loaded = true;
      }

      m_last_state = wxMEDIASTATE_PAUSED;
      QueuePauseEvent();
      break;
    case GST_PLAYER_STATE_PLAYING:
      m_last_state = wxMEDIASTATE_PLAYING;
      QueuePlayEvent();
      break;
    case GST_PLAYER_STATE_STOPPED:
    default:
      m_last_state = wxMEDIASTATE_STOPPED;
      QueueStopEvent();
      break;
  }
}

void wxGStreamerMediaBackend::EndOfStream()
{
    if (SendStopEvent())
        QueueFinishEvent();
}

bool wxGStreamerMediaBackend::SetPosition(wxLongLong where)
{
    gst_player_seek(m_player, where.GetValue() * GST_MSECOND);

    return true;
}

wxLongLong wxGStreamerMediaBackend::GetPosition()
{
    GstClockTime position = gst_player_get_position(m_player);

    return GST_CLOCK_TIME_IS_VALID(position) ? position / GST_MSECOND : 0;
}

wxLongLong wxGStreamerMediaBackend::GetDuration()
{
    GstClockTime duration = gst_player_get_duration(m_player);

    return GST_CLOCK_TIME_IS_VALID(duration) ? duration / GST_MSECOND : 0;
}

void wxGStreamerMediaBackend::Move(int WXUNUSED(x), int WXUNUSED(y), int WXUNUSED(w), int WXUNUSED(h))
{
    /* Nothing to be done here, at least for GTK+. For other toolkits we might
     * have to call
     * gst_player_video_overlay_video_renderer_set_render_rectangle() here
     */
}

wxSize wxGStreamerMediaBackend::GetVideoSize() const
{
    return m_videoSize;
}

double wxGStreamerMediaBackend::GetPlaybackRate()
{
    return gst_player_get_rate(m_player);
}

bool wxGStreamerMediaBackend::SetPlaybackRate(double dRate)
{
    gst_player_set_rate(m_player, dRate);
    return true;
}

wxMediaState wxGStreamerMediaBackend::GetState()
{
    return m_last_state;
}

bool wxGStreamerMediaBackend::SetVolume(double dVolume)
{
    gst_player_set_volume(m_player, dVolume);
    return true;
}

double wxGStreamerMediaBackend::GetVolume()
{
    return gst_player_get_volume(m_player);
}

wxLongLong wxGStreamerMediaBackend::GetDownloadProgress()
{
    return 0;
}

wxLongLong wxGStreamerMediaBackend::GetDownloadTotal()
{
    return 0;
}

// Force link into main library so this backend can be loaded
#include "wx/html/forcelnk.h"
FORCE_LINK_ME(basewxmediabackends)

#endif // wxUSE_MEDIACTRL && wxUSE_GSTREAMER && wxUSE_GSTREAMER_PLAYER
