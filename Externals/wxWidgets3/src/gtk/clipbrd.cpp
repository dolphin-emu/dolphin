/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/clipbrd.cpp
// Purpose:     wxClipboard implementation for wxGTK
// Author:      Robert Roebling, Vadim Zeitlin
// Copyright:   (c) 1998 Robert Roebling
//              (c) 2007 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_CLIPBOARD

#include "wx/clipbrd.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/dataobj.h"
#endif

#include "wx/scopedarray.h"
#include "wx/scopeguard.h"
#include "wx/evtloop.h"

#include "wx/gtk/private.h"

typedef wxScopedArray<wxDataFormat> wxDataFormatArray;

// ----------------------------------------------------------------------------
// data
// ----------------------------------------------------------------------------

static GdkAtom  g_clipboardAtom   = 0;
static GdkAtom  g_targetsAtom     = 0;
static GdkAtom  g_timestampAtom   = 0;

#if wxUSE_UNICODE
extern GdkAtom g_altTextAtom;
#endif

// the trace mask we use with wxLogTrace() - call
// wxLog::AddTraceMask(TRACE_CLIPBOARD) to enable the trace messages from here
// (there will be a *lot* of them!)
#define TRACE_CLIPBOARD wxT("clipboard")

// ----------------------------------------------------------------------------
// wxClipboardSync: used to perform clipboard operations synchronously
// ----------------------------------------------------------------------------

// constructing this object on stack will wait wait until the latest clipboard
// operation is finished on block exit
//
// notice that there can be no more than one such object alive at any moment,
// i.e. reentrancies are not allowed
class wxClipboardSync
{
public:
    wxClipboardSync(wxClipboard& clipboard)
    {
        wxASSERT_MSG( !ms_clipboard, wxT("reentrancy in clipboard code") );
        ms_clipboard = &clipboard;
    }

    ~wxClipboardSync()
    {
#if wxUSE_CONSOLE_EVENTLOOP
        // ensure that there is a running event loop: this might not be the
        // case if we're called before the main event loop startup
        wxEventLoopGuarantor ensureEventLoop;
#endif
        while (ms_clipboard)
            wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_CLIPBOARD);
    }

    // this method must be called by GTK+ callbacks to indicate that we got the
    // result for our clipboard operation
    static void OnDone(wxClipboard * WXUNUSED_UNLESS_DEBUG(clipboard))
    {
        wxASSERT_MSG( clipboard == ms_clipboard,
                        wxT("got notification for alien clipboard") );

        ms_clipboard = NULL;
    }

    // this method should be called if it's possible that no async clipboard
    // operation is currently in progress (like it can be the case when the
    // clipboard is cleared but not because we asked about it), it should only
    // be called if such situation is expected -- otherwise call OnDone() which
    // would assert in this case
    static void OnDoneIfInProgress(wxClipboard *clipboard)
    {
        if ( ms_clipboard )
            OnDone(clipboard);
    }

private:
    static wxClipboard *ms_clipboard;

    wxDECLARE_NO_COPY_CLASS(wxClipboardSync);
};

wxClipboard *wxClipboardSync::ms_clipboard = NULL;

// ============================================================================
// clipboard callbacks implementation
// ============================================================================

//-----------------------------------------------------------------------------
// "selection_received" for targets
//-----------------------------------------------------------------------------

extern "C" {
static void
targets_selection_received( GtkWidget *WXUNUSED(widget),
                            GtkSelectionData *selection_data,
                            guint32 WXUNUSED(time),
                            wxClipboard *clipboard )
{
    if ( !clipboard )
        return;

    wxON_BLOCK_EXIT1(wxClipboardSync::OnDone, clipboard);

    if (!selection_data)
        return;

    const int selection_data_length = gtk_selection_data_get_length(selection_data);
    if (selection_data_length <= 0)
        return;

    // make sure we got the data in the correct form
    GdkAtom type = gtk_selection_data_get_data_type(selection_data);
    if ( type != GDK_SELECTION_TYPE_ATOM )
    {
        if ( strcmp(wxGtkString(gdk_atom_name(type)), "TARGETS") != 0 )
        {
            wxLogTrace( TRACE_CLIPBOARD,
                        wxT("got unsupported clipboard target") );

            return;
        }
    }

    // it's not really a format, of course, but we can reuse its GetId() method
    // to format this atom as string
    wxDataFormat clip(gtk_selection_data_get_selection(selection_data));
    wxLogTrace( TRACE_CLIPBOARD,
                wxT("Received available formats for clipboard %s"),
                clip.GetId().c_str() );

    // the atoms we received, holding a list of targets (= formats)
    const GdkAtom* const atoms = (GdkAtom*)gtk_selection_data_get_data(selection_data);
    for (size_t i = 0; i < selection_data_length / sizeof(GdkAtom); i++)
    {
        const wxDataFormat format(atoms[i]);

        wxLogTrace(TRACE_CLIPBOARD, wxT("\t%s"), format.GetId().c_str());

        if ( clipboard->GTKOnTargetReceived(format) )
            return;
    }
}
}

bool wxClipboard::GTKOnTargetReceived(const wxDataFormat& format)
{
    if ( format != m_targetRequested )
        return false;

    m_formatSupported = true;
    return true;
}

//-----------------------------------------------------------------------------
// "selection_received" for the actual data
//-----------------------------------------------------------------------------

extern "C" {
static void
selection_received( GtkWidget *WXUNUSED(widget),
                    GtkSelectionData *selection_data,
                    guint32 WXUNUSED(time),
                    wxClipboard *clipboard )
{
    if ( !clipboard )
        return;

    wxON_BLOCK_EXIT1(wxClipboardSync::OnDone, clipboard);

    if (!selection_data || gtk_selection_data_get_length(selection_data) <= 0)
        return;

    clipboard->GTKOnSelectionReceived(*selection_data);
}
}

//-----------------------------------------------------------------------------
// "selection_clear"
//-----------------------------------------------------------------------------

extern "C" {
static gint
selection_clear_clip( GtkWidget *WXUNUSED(widget), GdkEventSelection *event )
{
    wxClipboard * const clipboard = wxTheClipboard;
    if ( !clipboard )
        return TRUE;

    // notice the use of OnDoneIfInProgress() here instead of just OnDone():
    // it's perfectly possible that we're receiving this notification from GTK+
    // even though we hadn't cleared the clipboard ourselves but because
    // another application (or even another window in the same program)
    // acquired it
    wxON_BLOCK_EXIT1(wxClipboardSync::OnDoneIfInProgress, clipboard);

    wxClipboard::Kind kind;
    if (event->selection == GDK_SELECTION_PRIMARY)
    {
        wxLogTrace(TRACE_CLIPBOARD, wxT("Lost primary selection" ));

        kind = wxClipboard::Primary;
    }
    else if (event->selection == g_clipboardAtom)
    {
        wxLogTrace(TRACE_CLIPBOARD, wxT("Lost clipboard" ));

        kind = wxClipboard::Clipboard;
    }
    else // some other selection, we're not concerned
    {
        return FALSE;
    }

    // the clipboard is no longer in our hands, we don't need data any more
    clipboard->GTKClearData(kind);

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// selection handler for supplying data
//-----------------------------------------------------------------------------

extern "C" {
static void
selection_handler( GtkWidget *WXUNUSED(widget),
                   GtkSelectionData *selection_data,
                   guint WXUNUSED(info),
                   guint WXUNUSED(time),
                   gpointer signal_data )
{
    wxClipboard * const clipboard = wxTheClipboard;
    if ( !clipboard )
        return;

    wxDataObject * const data = clipboard->GTKGetDataObject(
        gtk_selection_data_get_selection(selection_data));
    if ( !data )
        return;

    // ICCCM says that TIMESTAMP is a required atom.
    // In particular, it satisfies Klipper, which polls
    // TIMESTAMP to see if the clipboards content has changed.
    // It shall return the time which was used to set the data.
    if (gtk_selection_data_get_target(selection_data) == g_timestampAtom)
    {
        guint timestamp = GPOINTER_TO_UINT (signal_data);
        gtk_selection_data_set(selection_data,
                               GDK_SELECTION_TYPE_INTEGER,
                               32,
                               (guchar*)&(timestamp),
                               sizeof(timestamp));
        wxLogTrace(TRACE_CLIPBOARD,
                   wxT("Clipboard TIMESTAMP requested, returning timestamp=%u"),
                   timestamp);
        return;
    }

    wxDataFormat format(gtk_selection_data_get_target(selection_data));

    wxLogTrace(TRACE_CLIPBOARD,
               wxT("clipboard data in format %s, GtkSelectionData is target=%s type=%s selection=%s timestamp=%u"),
               format.GetId().c_str(),
               wxString::FromAscii(wxGtkString(gdk_atom_name(gtk_selection_data_get_target(selection_data)))).c_str(),
               wxString::FromAscii(wxGtkString(gdk_atom_name(gtk_selection_data_get_data_type(selection_data)))).c_str(),
               wxString::FromAscii(wxGtkString(gdk_atom_name(gtk_selection_data_get_selection(selection_data)))).c_str(),
               GPOINTER_TO_UINT( signal_data )
               );

    if ( !data->IsSupportedFormat( format ) )
        return;

    int size = data->GetDataSize( format );
    if ( !size )
        return;

    wxCharBuffer buf(size - 1); // it adds 1 internally (for NUL)

    // text data must be returned in UTF8 if format is wxDF_UNICODETEXT
    if ( !data->GetDataHere(format, buf.data()) )
        return;

    // use UTF8_STRING format if requested in Unicode build but just plain
    // STRING one in ANSI or if explicitly asked in Unicode
#if wxUSE_UNICODE
    if (format == wxDataFormat(wxDF_UNICODETEXT))
    {
        gtk_selection_data_set_text(
            selection_data,
            (const gchar*)buf.data(),
            size );
    }
    else
#endif // wxUSE_UNICODE
    {
        gtk_selection_data_set(
            selection_data,
            format.GetFormatId(),
            8*sizeof(gchar),
            (const guchar*)buf.data(),
            size );
    }
}
}

void wxClipboard::GTKOnSelectionReceived(const GtkSelectionData& sel)
{
    wxCHECK_RET( m_receivedData, wxT("should be inside GetData()") );

    const wxDataFormat format(gtk_selection_data_get_target(const_cast<GtkSelectionData*>(&sel)));
    wxLogTrace(TRACE_CLIPBOARD, wxT("Received selection %s"),
               format.GetId().c_str());

    if ( !m_receivedData->IsSupportedFormat(format, wxDataObject::Set) )
        return;

    m_receivedData->SetData(format,
        gtk_selection_data_get_length(const_cast<GtkSelectionData*>(&sel)),
        gtk_selection_data_get_data(const_cast<GtkSelectionData*>(&sel)));
    m_formatSupported = true;
}

//-----------------------------------------------------------------------------
// asynchronous "selection_received" for targets
//-----------------------------------------------------------------------------

extern "C" {
static void
async_targets_selection_received( GtkWidget *WXUNUSED(widget),
                            GtkSelectionData *selection_data,
                            guint32 WXUNUSED(time),
                            wxClipboard *clipboard )
{
    if ( !clipboard ) // Assert?
        return;

    if (!clipboard->m_sink)
        return;

    wxClipboardEvent *event = new wxClipboardEvent(wxEVT_CLIPBOARD_CHANGED);
    event->SetEventObject( clipboard );

    int selection_data_length = 0;
    if (selection_data)
        selection_data_length = gtk_selection_data_get_length(selection_data);

    if (selection_data_length <= 0)
    {
        clipboard->m_sink->QueueEvent( event );
        clipboard->m_sink.Release();
        return;
    }

    // make sure we got the data in the correct form
    GdkAtom type = gtk_selection_data_get_data_type(selection_data);
    if ( type != GDK_SELECTION_TYPE_ATOM )
    {
        if ( strcmp(wxGtkString(gdk_atom_name(type)), "TARGETS") != 0 )
        {
            wxLogTrace( TRACE_CLIPBOARD,
                        wxT("got unsupported clipboard target") );

            clipboard->m_sink->QueueEvent( event );
            clipboard->m_sink.Release();
            return;
        }
    }

    // it's not really a format, of course, but we can reuse its GetId() method
    // to format this atom as string
    wxDataFormat clip(gtk_selection_data_get_selection(selection_data));
    wxLogTrace( TRACE_CLIPBOARD,
                wxT("Received available formats for clipboard %s"),
                clip.GetId().c_str() );

    // the atoms we received, holding a list of targets (= formats)
    const GdkAtom* const atoms = (GdkAtom*)gtk_selection_data_get_data(selection_data);
    for (size_t i = 0; i < selection_data_length / sizeof(GdkAtom); i++)
    {
        const wxDataFormat format(atoms[i]);

        wxLogTrace(TRACE_CLIPBOARD, wxT("\t%s"), format.GetId().c_str());

        event->AddFormat( format );
    }

    clipboard->m_sink->QueueEvent( event );
    clipboard->m_sink.Release();
}
}

// ============================================================================
// wxClipboard implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxClipboard ctor/dtor
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxClipboard,wxObject);

wxClipboard::wxClipboard()
{
    m_idSelectionGetHandler = 0;

    m_open = false;

    m_dataPrimary =
    m_dataClipboard =
    m_receivedData = NULL;

    m_formatSupported = false;
    m_targetRequested = 0;

    // we use m_targetsWidget to query what formats are available
    m_targetsWidget = gtk_window_new( GTK_WINDOW_POPUP );
    gtk_widget_realize( m_targetsWidget );

    g_signal_connect (m_targetsWidget, "selection_received",
                      G_CALLBACK (targets_selection_received), this);

    // we use m_targetsWidgetAsync to query what formats are available asynchronously
    m_targetsWidgetAsync = gtk_window_new( GTK_WINDOW_POPUP );
    gtk_widget_realize( m_targetsWidgetAsync );

    g_signal_connect (m_targetsWidgetAsync, "selection_received",
                      G_CALLBACK (async_targets_selection_received), this);

    // we use m_clipboardWidget to get and to offer data
    m_clipboardWidget = gtk_window_new( GTK_WINDOW_POPUP );
    gtk_widget_realize( m_clipboardWidget );

    g_signal_connect (m_clipboardWidget, "selection_received",
                      G_CALLBACK (selection_received), this);

    g_signal_connect (m_clipboardWidget, "selection_clear_event",
                      G_CALLBACK (selection_clear_clip), NULL);

    // initialize atoms we use if not done yet
    if ( !g_clipboardAtom )
        g_clipboardAtom = gdk_atom_intern( "CLIPBOARD", FALSE );
    if ( !g_targetsAtom )
        g_targetsAtom = gdk_atom_intern ("TARGETS", FALSE);
    if ( !g_timestampAtom )
        g_timestampAtom = gdk_atom_intern ("TIMESTAMP", FALSE);
}

wxClipboard::~wxClipboard()
{
    Clear();

    gtk_widget_destroy( m_clipboardWidget );
    gtk_widget_destroy( m_targetsWidget );
}

// ----------------------------------------------------------------------------
// wxClipboard helper functions
// ----------------------------------------------------------------------------

GdkAtom wxClipboard::GTKGetClipboardAtom() const
{
    return m_usePrimary ? (GdkAtom)GDK_SELECTION_PRIMARY
                        : g_clipboardAtom;
}

void wxClipboard::GTKClearData(Kind kind)
{
    wxDataObject *&data = Data(kind);
    wxDELETE(data);
}

bool wxClipboard::SetSelectionOwner(bool set)
{
    bool rc = gtk_selection_owner_set
              (
                set ? m_clipboardWidget : NULL,
                GTKGetClipboardAtom(),
                (guint32)GDK_CURRENT_TIME
              ) != 0;

    if ( !rc )
    {
        wxLogTrace(TRACE_CLIPBOARD, wxT("Failed to %sset selection owner"),
                   set ? wxT("") : wxT("un"));
    }

    return rc;
}

void wxClipboard::AddSupportedTarget(GdkAtom atom)
{
    gtk_selection_add_target
    (
        m_clipboardWidget,
        GTKGetClipboardAtom(),
        atom,
        0 // info (same as client data) unused
    );
}

bool wxClipboard::IsSupportedAsync(wxEvtHandler *sink)
{
    if (m_sink.get())
        return false;  // currently busy, come back later

    wxCHECK_MSG( sink, false, wxT("no sink given") );

    m_sink = sink;
    gtk_selection_convert( m_targetsWidgetAsync,
                           GTKGetClipboardAtom(),
                           g_targetsAtom,
                           (guint32) GDK_CURRENT_TIME );

    return true;
}

bool wxClipboard::DoIsSupported(const wxDataFormat& format)
{
    wxCHECK_MSG( format, false, wxT("invalid clipboard format") );

    wxLogTrace(TRACE_CLIPBOARD, wxT("Checking if format %s is available"),
               format.GetId().c_str());

    // these variables will be used by our GTKOnTargetReceived()
    m_targetRequested = format;
    m_formatSupported = false;

    // block until m_formatSupported is set from targets_selection_received
    // callback
    {
        wxClipboardSync sync(*this);

        gtk_selection_convert( m_targetsWidget,
                               GTKGetClipboardAtom(),
                               g_targetsAtom,
                               (guint32) GDK_CURRENT_TIME );
    }

    return m_formatSupported;
}

// ----------------------------------------------------------------------------
// wxClipboard public API implementation
// ----------------------------------------------------------------------------

void wxClipboard::Clear()
{
    gtk_selection_clear_targets( m_clipboardWidget, GTKGetClipboardAtom() );

    if ( gdk_selection_owner_get(GTKGetClipboardAtom()) ==
            gtk_widget_get_window(m_clipboardWidget) )
    {
        wxClipboardSync sync(*this);

        // this will result in selection_clear_clip callback being called and
        // it will free our data
        SetSelectionOwner(false);
    }

    m_targetRequested = 0;
    m_formatSupported = false;
}

bool wxClipboard::Open()
{
    wxCHECK_MSG( !m_open, false, wxT("clipboard already open") );

    m_open = true;

    return true;
}

bool wxClipboard::SetData( wxDataObject *data )
{
    wxCHECK_MSG( m_open, false, wxT("clipboard not open") );

    wxCHECK_MSG( data, false, wxT("data is invalid") );

    Clear();

    return AddData( data );
}

bool wxClipboard::AddData( wxDataObject *data )
{
    wxCHECK_MSG( m_open, false, wxT("clipboard not open") );

    wxCHECK_MSG( data, false, wxT("data is invalid") );

    // we can only store one wxDataObject so clear the old one
    Clear();

    Data() = data;

    // get formats from wxDataObjects
    const size_t count = data->GetFormatCount();
    wxDataFormatArray formats(count);
    data->GetAllFormats(formats.get());

    // always provide TIMESTAMP as a target, see comments in selection_handler
    // for explanation
    AddSupportedTarget(g_timestampAtom);

    for ( size_t i = 0; i < count; i++ )
    {
        const wxDataFormat format(formats[i]);

        wxLogTrace(TRACE_CLIPBOARD, wxT("Adding support for %s"),
                   format.GetId().c_str());

        AddSupportedTarget(format);
    }

    if ( !m_idSelectionGetHandler )
    {
        m_idSelectionGetHandler = g_signal_connect (
                      m_clipboardWidget, "selection_get",
                      G_CALLBACK (selection_handler),
                      GUINT_TO_POINTER (gtk_get_current_event_time()) );
    }

    // tell the world we offer clipboard data
    return SetSelectionOwner();
}

void wxClipboard::Close()
{
    wxCHECK_RET( m_open, wxT("clipboard not open") );

    m_open = false;
}

bool wxClipboard::IsOpened() const
{
    return m_open;
}

bool wxClipboard::IsSupported( const wxDataFormat& format )
{
    if ( DoIsSupported(format) )
        return true;

#if wxUSE_UNICODE
    if ( format == wxDF_UNICODETEXT )
    {
        // also with plain STRING format
        return DoIsSupported(g_altTextAtom);
    }
#endif // wxUSE_UNICODE

    return false;
}

bool wxClipboard::GetData( wxDataObject& data )
{
    wxCHECK_MSG( m_open, false, wxT("clipboard not open") );

    // get all supported formats from wxDataObjects: notice that we are setting
    // the object data, so we need them in "Set" direction
    const size_t count = data.GetFormatCount(wxDataObject::Set);
    wxDataFormatArray formats(count);
    data.GetAllFormats(formats.get(), wxDataObject::Set);

    for ( size_t i = 0; i < count; i++ )
    {
        const wxDataFormat format(formats[i]);

        // is this format supported by clipboard ?
        if ( !DoIsSupported(format) )
            continue;

        wxLogTrace(TRACE_CLIPBOARD, wxT("Requesting format %s"),
                   format.GetId().c_str());

        // these variables will be used by our GTKOnSelectionReceived()
        m_receivedData = &data;
        m_formatSupported = false;

        {
            wxClipboardSync sync(*this);

            gtk_selection_convert(m_clipboardWidget,
                                  GTKGetClipboardAtom(),
                                  format,
                                  (guint32) GDK_CURRENT_TIME );
        } // wait until we get the results

        /*
           Normally this is a true error as we checked for the presence of such
           data before, but there are applications that may return an empty
           string (e.g. Gnumeric-1.6.1 on Linux if an empty cell is copied)
           which would produce a false error message here, so we check for the
           size of the string first. With ANSI, GetDataSize returns an extra
           value (for the closing null?), with unicode, the exact number of
           tokens is given (that is more than 1 for non-ASCII characters)
           (tested with Gnumeric-1.6.1 and OpenOffice.org-2.0.2)
         */
#if wxUSE_UNICODE
        if ( format != wxDF_UNICODETEXT || data.GetDataSize(format) > 0 )
#else // !UNICODE
        if ( format != wxDF_TEXT || data.GetDataSize(format) > 1 )
#endif // UNICODE / !UNICODE
        {
            wxCHECK_MSG( m_formatSupported, false,
                         wxT("error retrieving data from clipboard") );
        }

        return true;
    }

    wxLogTrace(TRACE_CLIPBOARD, wxT("GetData(): format not found"));

    return false;
}

wxDataObject* wxClipboard::GTKGetDataObject( GdkAtom atom )
{
    if ( atom == GDK_NONE )
        return Data();

    if ( atom == GDK_SELECTION_PRIMARY )
    {
        wxLogTrace(TRACE_CLIPBOARD, wxT("Primary selection requested" ));

        return Data( wxClipboard::Primary );
    }
    else if ( atom == g_clipboardAtom )
    {
        wxLogTrace(TRACE_CLIPBOARD, wxT("Clipboard data requested" ));

        return Data( wxClipboard::Clipboard );
    }
    else // some other selection, we're not concerned
    {
        return (wxDataObject*)NULL;
    }
}

#endif // wxUSE_CLIPBOARD
