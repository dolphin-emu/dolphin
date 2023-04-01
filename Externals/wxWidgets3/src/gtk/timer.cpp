/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/timer.cpp
// Purpose:     wxTimer implementation
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_TIMER

#include "wx/gtk/private/timer.h"
#include "wx/app.h"

#include <gtk/gtk.h>

// ----------------------------------------------------------------------------
// wxTimerImpl
// ----------------------------------------------------------------------------

extern "C" {

static gboolean timeout_callback(gpointer data)
{
    wxGTKTimerImpl *timer = (wxGTKTimerImpl*)data;

    const bool keepGoing = !timer->IsOneShot();
    if ( !keepGoing )
        timer->Stop();

    // When getting called from GDK's timer handler we
    // are no longer within GDK's grab on the GUI
    // thread so we must lock it here ourselves.
    gdk_threads_enter();

    timer->Notify();

    // Release lock again.
    gdk_threads_leave();

    wxApp* app = wxTheApp;
    if (app)
        app->WakeUpIdle();

    return keepGoing;
}

} // extern "C"

bool wxGTKTimerImpl::Start(int millisecs, bool oneShot)
{
    if ( !wxTimerImpl::Start(millisecs, oneShot) )
        return false;

    wxASSERT_MSG( !m_sourceId, wxT("shouldn't be still running") );

    m_sourceId = g_timeout_add(m_milli, timeout_callback, this);

    return true;
}

void wxGTKTimerImpl::Stop()
{
    wxASSERT_MSG( m_sourceId, wxT("should be running") );

    g_source_remove(m_sourceId);
    m_sourceId = 0;
}

#endif // wxUSE_TIMER

