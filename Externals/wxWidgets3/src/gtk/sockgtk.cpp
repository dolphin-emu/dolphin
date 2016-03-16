///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/sockgtk.cpp
// Purpose:     implementation of wxGTK-specific socket event handling
// Author:      Guilhem Lavaux, Vadim Zeitlin
// Created:     1999
// Copyright:   (c) 1999, 2007 wxWidgets dev team
//              (c) 2009 Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SOCKETS && defined(__UNIX__)

#include "wx/apptrait.h"
#include "wx/private/fdiomanager.h"

#include <glib.h>

extern "C" {
static gboolean wxSocket_Input(GIOChannel*, GIOCondition condition, gpointer data)
{
    wxFDIOHandler * const handler = static_cast<wxFDIOHandler *>(data);

    if (condition & G_IO_IN)
    {
        handler->OnReadWaiting();

        // we could have lost connection while reading in which case we
        // shouldn't call OnWriteWaiting() as the socket is now closed and it
        // would assert
        if ( !handler->IsOk() )
            return true;
    }

    if (condition & G_IO_OUT)
        handler->OnWriteWaiting();

    return true;
}
}

class GTKFDIOManager : public wxFDIOManager
{
public:
    virtual int AddInput(wxFDIOHandler *handler, int fd, Direction d) wxOVERRIDE
    {
        GIOChannel* channel = g_io_channel_unix_new(fd);
        unsigned id = g_io_add_watch(
            channel,
            d == OUTPUT ? G_IO_OUT : G_IO_IN,
            wxSocket_Input,
            handler);
        g_io_channel_unref(channel);
        return id;
    }

    virtual void
    RemoveInput(wxFDIOHandler* WXUNUSED(handler), int fd, Direction WXUNUSED(d)) wxOVERRIDE
    {
        g_source_remove(fd);
    }
};

wxFDIOManager *wxGUIAppTraits::GetFDIOManager()
{
    static GTKFDIOManager s_manager;
    return &s_manager;
}

#endif // wxUSE_SOCKETS && __UNIX__
