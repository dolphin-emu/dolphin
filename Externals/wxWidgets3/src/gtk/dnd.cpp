///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dnd.cpp
// Purpose:     wxDropTarget class
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_DRAG_AND_DROP

#include "wx/dnd.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/window.h"
    #include "wx/gdicmn.h"
#endif

#include "wx/scopeguard.h"

#include <gtk/gtk.h>
#include "wx/gtk/private/gtk2-compat.h"

//----------------------------------------------------------------------------
// global data
//----------------------------------------------------------------------------

extern bool g_blockEventsOnDrag;

// the flags used for the last DoDragDrop()
static long gs_flagsForDrag = 0;

// the trace mask we use with wxLogTrace() - call
// wxLog::AddTraceMask(TRACE_DND) to enable the trace messages from here
// (there are quite a few of them, so don't enable this by default)
#define TRACE_DND "dnd"

// global variables because GTK+ DnD want to have the
// mouse event that caused it
extern GdkEvent *g_lastMouseEvent;
extern int       g_lastButtonNumber;

//----------------------------------------------------------------------------
// standard icons
//----------------------------------------------------------------------------

/* Copyright (c) Julian Smart */
static const char * page_xpm[] = {
/* columns rows colors chars-per-pixel */
"32 32 37 1",
"5 c #7198D9",
", c #769CDA",
"2 c #DCE6F6",
"i c #FFFFFF",
"e c #779DDB",
": c #9AB6E4",
"9 c #EAF0FA",
"- c #B1C7EB",
"$ c #6992D7",
"y c #F7F9FD",
"= c #BED0EE",
"q c #F0F5FC",
"; c #A8C0E8",
"@ c #366BC2",
"  c None",
"u c #FDFEFF",
"8 c #5987D3",
"* c #C4D5F0",
"7 c #7CA0DC",
"O c #487BCE",
"< c #6B94D7",
"& c #CCDAF2",
"> c #89A9DF",
"3 c #5584D1",
"w c #82A5DE",
"1 c #3F74CB",
"+ c #3A70CA",
". c #3569BF",
"% c #D2DFF4",
"# c #3366BB",
"r c #F5F8FD",
"0 c #FAFCFE",
"4 c #DFE8F7",
"X c #5E8AD4",
"o c #5282D0",
"t c #B8CCEC",
"6 c #E5EDF9",
/* pixels */
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"       .XXXooOO++@#             ",
"       $%&*=-;::>,<1            ",
"       $2%&*=-;::><:3           ",
"       $42%&*=-;::<&:3          ",
"       56477<<<<8<<9&:X         ",
"       59642%&*=-;<09&:5        ",
"       5q9642%&*=-<<<<<#        ",
"       5qqw777<<<<<88:>+        ",
"       erqq9642%&*=t;::+        ",
"       eyrqq9642%&*=t;:O        ",
"       eyywwww777<<<<t;O        ",
"       e0yyrqq9642%&*=to        ",
"       e00yyrqq9642%&*=o        ",
"       eu0wwwwwww777<&*X        ",
"       euu00yyrqq9642%&X        ",
"       eiuu00yyrqq9642%X        ",
"       eiiwwwwwwwwww742$        ",
"       eiiiuu00yyrqq964$        ",
"       eiiiiuu00yyrqq96$        ",
"       eiiiiiuu00yyrqq95        ",
"       eiiiiiiuu00yyrqq5        ",
"       eeeeeeeeeeeeee55e        ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                "
};


// ============================================================================
// private functions
// ============================================================================

// ----------------------------------------------------------------------------
// convert between GTK+ and wxWidgets DND constants
// ----------------------------------------------------------------------------

static wxDragResult ConvertFromGTK(long action)
{
    switch ( action )
    {
        case GDK_ACTION_COPY:
            return wxDragCopy;

        case GDK_ACTION_LINK:
            return wxDragLink;

        case GDK_ACTION_MOVE:
            return wxDragMove;
    }

    return wxDragNone;
}

// ----------------------------------------------------------------------------
// "drag_leave"
// ----------------------------------------------------------------------------

extern "C" {
static void target_drag_leave( GtkWidget *WXUNUSED(widget),
                               GdkDragContext *context,
                               guint WXUNUSED(time),
                               wxDropTarget *drop_target )
{
    /* inform the wxDropTarget about the current GdkDragContext.
       this is only valid for the duration of this call */
    drop_target->GTKSetDragContext( context );

    /* we don't need return values. this event is just for
       information */
    drop_target->OnLeave();

    /* this has to be done because GDK has no "drag_enter" event */
    drop_target->m_firstMotion = true;

    /* after this, invalidate the drop_target's GdkDragContext */
    drop_target->GTKSetDragContext( NULL );
}
}

// ----------------------------------------------------------------------------
// "drag_motion"
// ----------------------------------------------------------------------------

extern "C" {
static gboolean target_drag_motion( GtkWidget *WXUNUSED(widget),
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    guint time,
                                    wxDropTarget *drop_target )
{
    /* Owen Taylor: "if the coordinates not in a drop zone,
       return FALSE, otherwise call gtk_drag_status() and
       return TRUE" */

#if 0
    wxPrintf( "motion\n" );
    GList *tmp_list;
    for (tmp_list = context->targets; tmp_list; tmp_list = tmp_list->next)
    {
        wxString atom = wxString::FromAscii( gdk_atom_name (GDK_POINTER_TO_ATOM (tmp_list->data)) );
        wxPrintf( "Atom: %s\n", atom );
    }
#endif

    // Inform the wxDropTarget about the current GdkDragContext.
    // This is only valid for the duration of this call.
    drop_target->GTKSetDragContext( context );

    // Does the source actually accept the data type?
    if (drop_target->GTKGetMatchingPair() == (GdkAtom) 0)
    {
        drop_target->GTKSetDragContext( NULL );
        return FALSE;
    }

    wxDragResult suggested_action = drop_target->GTKFigureOutSuggestedAction();

    wxDragResult result = wxDragNone;

    if (drop_target->m_firstMotion)
    {
        // the first "drag_motion" event substitutes a "drag_enter" event
        result = drop_target->OnEnter( x, y, suggested_action );
    }
    else
    {
        // give program a chance to react (i.e. to say no by returning FALSE)
        result = drop_target->OnDragOver( x, y, suggested_action );
    }

    GdkDragAction result_action = GDK_ACTION_DEFAULT;
    if (result == wxDragCopy)
        result_action = GDK_ACTION_COPY;
    else if (result == wxDragLink)
        result_action = GDK_ACTION_LINK;
    else
        result_action = GDK_ACTION_MOVE;

    // is result action actually supported
    bool ret = (result_action != GDK_ACTION_DEFAULT) &&
               (gdk_drag_context_get_actions(context) & result_action);

    if (ret)
        gdk_drag_status( context, result_action, time );

    // after this, invalidate the drop_target's GdkDragContext
    drop_target->GTKSetDragContext( NULL );

    // this has to be done because GDK has no "drag_enter" event
    drop_target->m_firstMotion = false;

    return ret;
}
}

// ----------------------------------------------------------------------------
// "drag_drop"
// ----------------------------------------------------------------------------

extern "C" {
static gboolean target_drag_drop( GtkWidget *widget,
                                  GdkDragContext *context,
                                  gint x,
                                  gint y,
                                  guint time,
                                  wxDropTarget *drop_target )
{
    /* Owen Taylor: "if the drop is not in a drop zone,
       return FALSE, otherwise, if you aren't accepting
       the drop, call gtk_drag_finish() with success == FALSE
       otherwise call gtk_drag_data_get()" */

    /* inform the wxDropTarget about the current GdkDragContext.
       this is only valid for the duration of this call */
    drop_target->GTKSetDragContext( context );

    // Does the source actually accept the data type?
    if (drop_target->GTKGetMatchingPair() == (GdkAtom) 0)
    {
        // cancel the whole thing
        gtk_drag_finish( context,
                          FALSE,        // no success
                          FALSE,        // don't delete data on dropping side
                          time );

        drop_target->GTKSetDragContext( NULL );

        drop_target->m_firstMotion = true;

        return FALSE;
    }

    /* inform the wxDropTarget about the current drag widget.
       this is only valid for the duration of this call */
    drop_target->GTKSetDragWidget( widget );

    /* inform the wxDropTarget about the current drag time.
       this is only valid for the duration of this call */
    drop_target->GTKSetDragTime( time );

    /* reset the block here as someone might very well
       show a dialog as a reaction to a drop and this
       wouldn't work without events */
    g_blockEventsOnDrag = false;

    bool ret = drop_target->OnDrop( x, y );

    if (!ret)
    {
        wxLogTrace(TRACE_DND, wxT( "Drop target: OnDrop returned FALSE") );

        /* cancel the whole thing */
        gtk_drag_finish( context,
                          FALSE,        /* no success */
                          FALSE,        /* don't delete data on dropping side */
                          time );
    }
    else
    {
        wxLogTrace(TRACE_DND, wxT( "Drop target: OnDrop returned true") );

        GdkAtom format = drop_target->GTKGetMatchingPair();

        // this does happen somehow, see bug 555111
        wxCHECK_MSG( format, FALSE, wxT("no matching GdkAtom for format?") );

        /* this should trigger an "drag_data_received" event */
        gtk_drag_get_data( widget,
                           context,
                           format,
                           time );
    }

    /* after this, invalidate the drop_target's GdkDragContext */
    drop_target->GTKSetDragContext( NULL );

    /* after this, invalidate the drop_target's drag widget */
    drop_target->GTKSetDragWidget( NULL );

    /* this has to be done because GDK has no "drag_enter" event */
    drop_target->m_firstMotion = true;

    return ret;
}
}

// ----------------------------------------------------------------------------
// "drag_data_received"
// ----------------------------------------------------------------------------

extern "C" {
static void target_drag_data_received( GtkWidget *WXUNUSED(widget),
                                       GdkDragContext *context,
                                       gint x,
                                       gint y,
                                       GtkSelectionData *data,
                                       guint WXUNUSED(info),
                                       guint time,
                                       wxDropTarget *drop_target )
{
    /* Owen Taylor: "call gtk_drag_finish() with
       success == TRUE" */

    if (gtk_selection_data_get_length(data) <= 0 || gtk_selection_data_get_format(data) != 8)
    {
        /* negative data length and non 8-bit data format
           qualifies for junk */
        gtk_drag_finish (context, FALSE, FALSE, time);

        return;
    }

    wxLogTrace(TRACE_DND, wxT( "Drop target: data received event") );

    /* inform the wxDropTarget about the current GtkSelectionData.
       this is only valid for the duration of this call */
    drop_target->GTKSetDragData( data );

    wxDragResult result = ConvertFromGTK(gdk_drag_context_get_selected_action(context));

    if ( wxIsDragResultOk( drop_target->OnData( x, y, result ) ) )
    {
        wxLogTrace(TRACE_DND, wxT( "Drop target: OnData returned true") );

        /* tell GTK that data transfer was successful */
        gtk_drag_finish( context, TRUE, FALSE, time );
    }
    else
    {
        wxLogTrace(TRACE_DND, wxT( "Drop target: OnData returned FALSE") );

        /* tell GTK that data transfer was not successful */
        gtk_drag_finish( context, FALSE, FALSE, time );
    }

    /* after this, invalidate the drop_target's drag data */
    drop_target->GTKSetDragData( NULL );
}
}

//----------------------------------------------------------------------------
// wxDropTarget
//----------------------------------------------------------------------------

wxDropTarget::wxDropTarget( wxDataObject *data )
            : wxDropTargetBase( data )
{
    m_firstMotion = true;
    m_dragContext = NULL;
    m_dragWidget = NULL;
    m_dragData = NULL;
    m_dragTime = 0;
}

wxDragResult wxDropTarget::OnDragOver( wxCoord WXUNUSED(x),
                                       wxCoord WXUNUSED(y),
                                       wxDragResult def )
{
    return def;
}

bool wxDropTarget::OnDrop( wxCoord WXUNUSED(x), wxCoord WXUNUSED(y) )
{
    return true;
}

wxDragResult wxDropTarget::OnData( wxCoord WXUNUSED(x), wxCoord WXUNUSED(y),
                                   wxDragResult def )
{
    return GetData() ? def : wxDragNone;
}

wxDragResult wxDropTarget::GTKFigureOutSuggestedAction()
{
    if (!m_dragContext)
        return wxDragError;

    // GTK+ always supposes that we want to copy the data by default while we
    // might want to move it, so examine not only suggested_action - which is
    // only good if we don't have our own preferences - but also the actions
    // field
    wxDragResult suggested_action = wxDragNone;
    const GdkDragAction actions = gdk_drag_context_get_actions(m_dragContext);
    if (GetDefaultAction() == wxDragNone)
    {
        // use default action set by wxDropSource::DoDragDrop()
        if ( (gs_flagsForDrag & wxDrag_DefaultMove) == wxDrag_DefaultMove &&
            (actions & GDK_ACTION_MOVE))
        {
            // move is requested by the program and allowed by GTK+ - do it, even
            // though suggested_action may be currently wxDragCopy
            suggested_action = wxDragMove;
        }
        else // use whatever GTK+ says we should
        {
            suggested_action = ConvertFromGTK(gdk_drag_context_get_suggested_action(m_dragContext));

#if 0
            // RR: I don't understand the code below: if the drag comes from
            //     a different app, the gs_flagsForDrag is invalid; if it
            //     comes from the same wx app, then GTK+ hopefully won't
            //     suggest something we didn't allow in the frist place
            //     in DoDrop()
            if ( (suggested_action == wxDragMove) && !(gs_flagsForDrag & wxDrag_AllowMove) )
            {
                // we're requested to move but we can't
                suggested_action = wxDragCopy;
            }
#endif
        }
    }
    else if (GetDefaultAction() == wxDragMove &&
            (actions & GDK_ACTION_MOVE))
    {

        suggested_action = wxDragMove;
    }
    else
    {
        if (actions & GDK_ACTION_COPY)
            suggested_action = wxDragCopy;
        else if (actions & GDK_ACTION_MOVE)
            suggested_action = wxDragMove;
        else if (actions & GDK_ACTION_LINK)
            suggested_action = wxDragLink;
        else
            suggested_action = wxDragNone;
    }

    return suggested_action;
}

wxDataFormat wxDropTarget::GetMatchingPair()
{
    return wxDataFormat( GTKGetMatchingPair() );
}

GdkAtom wxDropTarget::GTKGetMatchingPair(bool quiet)
{
    if (!m_dataObject)
        return (GdkAtom) 0;

    if (!m_dragContext)
        return (GdkAtom) 0;

    const GList* child = gdk_drag_context_list_targets(m_dragContext);
    while (child)
    {
        GdkAtom formatAtom = (GdkAtom)(child->data);
        wxDataFormat format( formatAtom );

        if ( !quiet )
        {
            wxLogTrace(TRACE_DND, wxT("Drop target: drag has format: %s"),
                       format.GetId().c_str());
        }

        if (m_dataObject->IsSupportedFormat( format ))
            return formatAtom;

        child = child->next;
    }

    return (GdkAtom) 0;
}

bool wxDropTarget::GetData()
{
    if (!m_dragData)
        return false;

    if (!m_dataObject)
        return false;

    wxDataFormat dragFormat(gtk_selection_data_get_target(m_dragData));

    if (!m_dataObject->IsSupportedFormat( dragFormat ))
        return false;

    m_dataObject->SetData(dragFormat,
        (size_t)gtk_selection_data_get_length(m_dragData),
        (const void*)gtk_selection_data_get_data(m_dragData));

    return true;
}

void wxDropTarget::GtkUnregisterWidget( GtkWidget *widget )
{
    wxCHECK_RET( widget != NULL, wxT("unregister widget is NULL") );

    gtk_drag_dest_unset( widget );

    g_signal_handlers_disconnect_by_func (widget,
                                          (gpointer) target_drag_leave, this);
    g_signal_handlers_disconnect_by_func (widget,
                                          (gpointer) target_drag_motion, this);
    g_signal_handlers_disconnect_by_func (widget,
                                          (gpointer) target_drag_drop, this);
    g_signal_handlers_disconnect_by_func (widget,
                                          (gpointer) target_drag_data_received, this);
}

void wxDropTarget::GtkRegisterWidget( GtkWidget *widget )
{
    wxCHECK_RET( widget != NULL, wxT("register widget is NULL") );

    /* gtk_drag_dest_set() determines what default behaviour we'd like
       GTK to supply. we don't want to specify out targets (=formats)
       or actions in advance (i.e. not GTK_DEST_DEFAULT_MOTION and
       not GTK_DEST_DEFAULT_DROP). instead we react individually to
       "drag_motion" and "drag_drop" events. this makes it possible
       to allow dropping on only a small area. we should set
       GTK_DEST_DEFAULT_HIGHLIGHT as this will switch on the nice
       highlighting if dragging over standard controls, but this
       seems to be broken without the other two. */

    gtk_drag_dest_set( widget,
                       (GtkDestDefaults) 0,         /* no default behaviour */
                       NULL,      /* we don't supply any formats here */
                       0,                           /* number of targets = 0 */
                       (GdkDragAction) 0 );         /* we don't supply any actions here */

    g_signal_connect (widget, "drag_leave",
                      G_CALLBACK (target_drag_leave), this);

    g_signal_connect (widget, "drag_motion",
                      G_CALLBACK (target_drag_motion), this);

    g_signal_connect (widget, "drag_drop",
                      G_CALLBACK (target_drag_drop), this);

    g_signal_connect (widget, "drag_data_received",
                      G_CALLBACK (target_drag_data_received), this);
}

//----------------------------------------------------------------------------
// "drag_data_get"
//----------------------------------------------------------------------------

extern "C" {
static void
source_drag_data_get  (GtkWidget          *WXUNUSED(widget),
                       GdkDragContext     *context,
                       GtkSelectionData   *selection_data,
                       guint               WXUNUSED(info),
                       guint               WXUNUSED(time),
                       wxDropSource       *drop_source )
{
    wxDataFormat format(gtk_selection_data_get_target(selection_data));

    wxLogTrace(TRACE_DND, wxT("Drop source: format requested: %s"),
               format.GetId().c_str());

    drop_source->m_retValue = wxDragError;

    wxDataObject *data = drop_source->GetDataObject();

    if (!data)
    {
        wxLogTrace(TRACE_DND, wxT("Drop source: no data object") );
        return;
    }

    if (!data->IsSupportedFormat(format))
    {
        wxLogTrace(TRACE_DND, wxT("Drop source: unsupported format") );
        return;
    }

    if (data->GetDataSize(format) == 0)
    {
        wxLogTrace(TRACE_DND, wxT("Drop source: empty data") );
        return;
    }

    size_t size = data->GetDataSize(format);

//  printf( "data size: %d.\n", (int)data_size );

    guchar *d = new guchar[size];

    if (!data->GetDataHere( format, (void*)d ))
    {
        delete[] d;
        return;
    }

    drop_source->m_retValue = ConvertFromGTK(gdk_drag_context_get_selected_action(context));

    gtk_selection_data_set( selection_data,
                            gtk_selection_data_get_target(selection_data),
                            8,   // 8-bit
                            d,
                            size );

    delete[] d;
}
}

//----------------------------------------------------------------------------
// "drag_end"
//----------------------------------------------------------------------------

extern "C" {
static void source_drag_end( GtkWidget          *WXUNUSED(widget),
                             GdkDragContext     *WXUNUSED(context),
                             wxDropSource       *drop_source )
{
    drop_source->m_waiting = false;
}
}

//-----------------------------------------------------------------------------
// "configure_event" from m_iconWindow
//-----------------------------------------------------------------------------

extern "C" {
static gint
gtk_dnd_window_configure_callback( GtkWidget *WXUNUSED(widget), GdkEventConfigure *WXUNUSED(event), wxDropSource *source )
{
    source->GiveFeedback(ConvertFromGTK(gdk_drag_context_get_selected_action(source->m_dragContext)));

    return 0;
}
}

//---------------------------------------------------------------------------
// wxDropSource
//---------------------------------------------------------------------------

wxDropSource::wxDropSource(wxWindow *win,
                           const wxIcon &iconCopy,
                           const wxIcon &iconMove,
                           const wxIcon &iconNone)
{
    m_waiting = true;

    m_iconWindow = NULL;

    m_window = win;
    m_widget = win->m_widget;
    if (win->m_wxwindow) m_widget = win->m_wxwindow;

    m_retValue = wxDragNone;

    SetIcons(iconCopy, iconMove, iconNone);
}

wxDropSource::wxDropSource(wxDataObject& data,
                           wxWindow *win,
                           const wxIcon &iconCopy,
                           const wxIcon &iconMove,
                           const wxIcon &iconNone)
{
    m_waiting = true;

    SetData( data );

    m_iconWindow = NULL;

    m_window = win;
    m_widget = win->m_widget;
    if (win->m_wxwindow) m_widget = win->m_wxwindow;

    m_retValue = wxDragNone;

    SetIcons(iconCopy, iconMove, iconNone);
}

void wxDropSource::SetIcons(const wxIcon &iconCopy,
                            const wxIcon &iconMove,
                            const wxIcon &iconNone)
{
    m_iconCopy = iconCopy;
    m_iconMove = iconMove;
    m_iconNone = iconNone;

    if ( !m_iconCopy.IsOk() )
        m_iconCopy = wxIcon(page_xpm);
    if ( !m_iconMove.IsOk() )
        m_iconMove = m_iconCopy;
    if ( !m_iconNone.IsOk() )
        m_iconNone = m_iconCopy;
}

wxDropSource::~wxDropSource()
{
}

void wxDropSource::PrepareIcon( int action, GdkDragContext *context )
{
    // get the right icon to display
    wxIcon *icon = NULL;
    if ( action & GDK_ACTION_MOVE )
        icon = &m_iconMove;
    else if ( action & GDK_ACTION_COPY )
        icon = &m_iconCopy;
    else
        icon = &m_iconNone;

#ifndef __WXGTK3__
    GdkBitmap *mask;
    if ( icon->GetMask() )
        mask = *icon->GetMask();
    else
        mask = NULL;

    GdkPixmap *pixmap = icon->GetPixmap();

    GdkColormap *colormap = gtk_widget_get_colormap( m_widget );
    gtk_widget_push_colormap (colormap);
#endif

    m_iconWindow = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_widget_set_events (m_iconWindow, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    gtk_widget_set_app_paintable (m_iconWindow, TRUE);

#ifdef __WXGTK3__
    gtk_widget_set_visual(m_iconWindow, gtk_widget_get_visual(m_widget));
#else
    gtk_widget_pop_colormap ();
#endif

    gtk_widget_set_size_request (m_iconWindow, icon->GetWidth(), icon->GetHeight());
    gtk_widget_realize (m_iconWindow);

    g_signal_connect (m_iconWindow, "configure_event",
                      G_CALLBACK (gtk_dnd_window_configure_callback), this);

#ifdef __WXGTK3__
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window(m_iconWindow));
    icon->SetSourceSurface(cr, 0, 0);
    cairo_pattern_t* pattern = cairo_get_source(cr);
    gdk_window_set_background_pattern(gtk_widget_get_window(m_iconWindow), pattern);
    cairo_destroy(cr);
    cairo_surface_t* mask = NULL;
    if (icon->GetMask())
        mask = *icon->GetMask();
    if (mask)
    {
        cairo_region_t* region = gdk_cairo_region_create_from_surface(mask);
        gtk_widget_shape_combine_region(m_iconWindow, region);
        cairo_region_destroy(region);
    }
#else
    gdk_window_set_back_pixmap(gtk_widget_get_window(m_iconWindow), pixmap, false);

    if (mask)
        gtk_widget_shape_combine_mask (m_iconWindow, mask, 0, 0);
#endif

    gtk_drag_set_icon_widget( context, m_iconWindow, 0, 0 );
}

wxDragResult wxDropSource::DoDragDrop(int flags)
{
    wxCHECK_MSG( m_data && m_data->GetFormatCount(), wxDragNone,
                 wxT("Drop source: no data") );

    // still in drag
    if (g_blockEventsOnDrag)
        return wxDragNone;

    // don't start dragging if no button is down
    if (g_lastButtonNumber == 0)
        return wxDragNone;

    // we can only start a drag after a mouse event
    if (g_lastMouseEvent == NULL)
        return wxDragNone;

    GTKConnectDragSignals();
    wxON_BLOCK_EXIT_OBJ0(*this, wxDropSource::GTKDisconnectDragSignals);

    m_waiting = true;

    GtkTargetList *target_list = gtk_target_list_new( NULL, 0 );

    wxDataFormat *array = new wxDataFormat[ m_data->GetFormatCount() ];
    m_data->GetAllFormats( array );
    size_t count = m_data->GetFormatCount();
    for (size_t i = 0; i < count; i++)
    {
        GdkAtom atom = array[i];
        wxLogTrace(TRACE_DND, wxT("Drop source: Supported atom %s"),
                   gdk_atom_name( atom ));
        gtk_target_list_add( target_list, atom, 0, 0 );
    }
    delete[] array;

    int allowed_actions = GDK_ACTION_COPY;
    if ( flags & wxDrag_AllowMove )
        allowed_actions |= GDK_ACTION_MOVE;

    // VZ: as we already use g_blockEventsOnDrag it shouldn't be that bad
    //     to use a global to pass the flags to the drop target but I'd
    //     surely prefer a better way to do it
    gs_flagsForDrag = flags;

    m_retValue = wxDragCancel;

    GdkDragContext *context = gtk_drag_begin( m_widget,
                target_list,
                (GdkDragAction)allowed_actions,
                g_lastButtonNumber,  // number of mouse button which started drag
                (GdkEvent*) g_lastMouseEvent );

    if ( !context )
    {
        // this can happen e.g. if gdk_pointer_grab() failed
        return wxDragError;
    }

    m_dragContext = context;

    PrepareIcon( allowed_actions, context );

    while (m_waiting)
        gtk_main_iteration();

    g_signal_handlers_disconnect_by_func (m_iconWindow,
                                          (gpointer) gtk_dnd_window_configure_callback, this);

    return m_retValue;
}

void wxDropSource::GTKConnectDragSignals()
{
    if (!m_widget)
        return;

    g_blockEventsOnDrag = true;

    g_signal_connect (m_widget, "drag_data_get",
                      G_CALLBACK (source_drag_data_get), this);
    g_signal_connect (m_widget, "drag_end",
                      G_CALLBACK (source_drag_end), this);

}

void wxDropSource::GTKDisconnectDragSignals()
{
    if (!m_widget)
        return;

    g_blockEventsOnDrag = false;

    g_signal_handlers_disconnect_by_func (m_widget,
                                          (gpointer) source_drag_data_get,
                                          this);
    g_signal_handlers_disconnect_by_func (m_widget,
                                          (gpointer) source_drag_end,
                                          this);
}

#endif
      // wxUSE_DRAG_AND_DROP
