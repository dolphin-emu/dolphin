/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/fswatcher_inotify.cpp
// Purpose:     inotify-based wxFileSystemWatcher implementation
// Author:      Bartosz Bekier
// Created:     2009-05-26
// RCS-ID:      $Id: fswatcher_inotify.cpp 64656 2010-06-20 18:18:23Z VZ $
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FSWATCHER

#include "wx/fswatcher.h"

#ifdef wxHAS_INOTIFY

#include <sys/inotify.h>
#include <unistd.h>
#include "wx/private/fswatcher.h"

// ============================================================================
// wxFSWatcherImpl implementation & helper wxFSWSourceHandler implementation
// ============================================================================

// inotify watch descriptor => wxFSWatchEntry* map
WX_DECLARE_HASH_MAP(int, wxFSWatchEntry*, wxIntegerHash, wxIntegerEqual,
                                              wxFSWatchEntryDescriptors);

// inotify event cookie => inotify_event* map
WX_DECLARE_HASH_MAP(int, inotify_event*, wxIntegerHash, wxIntegerEqual,
                                                      wxInotifyCookies);

/**
 * Helper class encapsulating inotify mechanism
 */
class wxFSWatcherImplUnix : public wxFSWatcherImpl
{
public:
    wxFSWatcherImplUnix(wxFileSystemWatcherBase* watcher) :
        wxFSWatcherImpl(watcher),
        m_source(NULL),
        m_ifd(-1)
    {
        m_handler = new wxFSWSourceHandler(this);
    }

    ~wxFSWatcherImplUnix()
    {
        // we close inotify only if initialized before
        if (IsOk())
        {
            Close();
        }

        delete m_handler;
    }

    bool Init()
    {
        wxCHECK_MSG( !IsOk(), false, "Inotify already initialized" );

        wxEventLoopBase *loop = wxEventLoopBase::GetActive();
        wxCHECK_MSG( loop, false, "File system watcher needs an event loop" );

        m_ifd = inotify_init();
        if ( m_ifd == -1 )
        {
            wxLogSysError( _("Unable to create inotify instance") );
            return false;
        }

        m_source = loop->AddSourceForFD
                         (
                          m_ifd,
                          m_handler,
                          wxEVENT_SOURCE_INPUT | wxEVENT_SOURCE_EXCEPTION
                         );

        return m_source != NULL;
    }

    void Close()
    {
        wxCHECK_RET( IsOk(),
                    "Inotify not initialized or invalid inotify descriptor" );

        wxDELETE(m_source);

        if ( close(m_ifd) != 0 )
        {
            wxLogSysError( _("Unable to close inotify instance") );
        }
    }

    virtual bool DoAdd(wxSharedPtr<wxFSWatchEntryUnix> watch)
    {
        wxCHECK_MSG( IsOk(), false,
                    "Inotify not initialized or invalid inotify descriptor" );

        int wd = DoAddInotify(watch.get());
        if (wd == -1)
        {
            wxLogSysError( _("Unable to add inotify watch") );
            return false;
        }

        wxFSWatchEntryDescriptors::value_type val(wd, watch.get());
        if (!m_watchMap.insert(val).second)
        {
            wxFAIL_MSG( wxString::Format( "Path %s is already watched",
                                           watch->GetPath()) );
            return false;
        }

        return true;
    }

    virtual bool DoRemove(wxSharedPtr<wxFSWatchEntryUnix> watch)
    {
        wxCHECK_MSG( IsOk(), false,
                    "Inotify not initialized or invalid inotify descriptor" );

        int ret = DoRemoveInotify(watch.get());
        if (ret == -1)
        {
            wxLogSysError( _("Unable to remove inotify watch") );
            return false;
        }

        if (m_watchMap.erase(watch->GetWatchDescriptor()) != 1)
        {
            wxFAIL_MSG( wxString::Format("Path %s is not watched",
                                          watch->GetPath()) );
        }
        watch->SetWatchDescriptor(-1);
        return true;
    }

    virtual bool RemoveAll()
    {
        wxFSWatchEntries::iterator it = m_watches.begin();
        for ( ; it != m_watches.end(); ++it )
        {
            (void) DoRemove(it->second);
        }
        m_watches.clear();
        return true;
    }

    int ReadEvents()
    {
        wxCHECK_MSG( IsOk(), -1,
                    "Inotify not initialized or invalid inotify descriptor" );

        // read events
        // TODO differentiate depending on params
        char buf[128 * sizeof(inotify_event)];
        int left = ReadEventsToBuf(buf, sizeof(buf));
        if (left == -1)
            return -1;

        // left > 0, we have events
        char* memory = buf;
        int event_count = 0;
        while (left > 0) // OPT checking 'memory' would suffice
        {
            event_count++;
            inotify_event* e = (inotify_event*)memory;

            // process one inotify_event
            ProcessNativeEvent(*e);

            int offset = sizeof(inotify_event) + e->len;
            left -= offset;
            memory += offset;
        }

        // take care of unmatched renames
        ProcessRenames();

        wxLogTrace(wxTRACE_FSWATCHER, "We had %d native events", event_count);
        return event_count;
    }

    bool IsOk() const
    {
        return m_source != NULL;
    }

protected:
    int DoAddInotify(wxFSWatchEntry* watch)
    {
        int flags = Watcher2NativeFlags(watch->GetFlags());
        int wd = inotify_add_watch(m_ifd, watch->GetPath().fn_str(), flags);
        // finally we can set watch descriptor
        watch->SetWatchDescriptor(wd);
        return wd;
    }

    int DoRemoveInotify(wxFSWatchEntry* watch)
    {
        return inotify_rm_watch(m_ifd, watch->GetWatchDescriptor());
    }

    void ProcessNativeEvent(const inotify_event& inevt)
    {
        wxLogTrace(wxTRACE_FSWATCHER, InotifyEventToString(inevt));

        // after removing inotify watch we get IN_IGNORED for it, but the watch
        // will be already removed from our list at that time
        if (inevt.mask & IN_IGNORED)
        {
            return;
        }

        // get watch entry for this event
        wxFSWatchEntryDescriptors::iterator it = m_watchMap.find(inevt.wd);
        wxCHECK_RET(it != m_watchMap.end(),
                             "Watch descriptor not present in the watch map!");

        wxFSWatchEntry& watch = *(it->second);
        int nativeFlags = inevt.mask;
        int flags = Native2WatcherFlags(nativeFlags);

        // check out for error/warning condition
        if (flags & wxFSW_EVENT_WARNING || flags & wxFSW_EVENT_ERROR)
        {
            wxString errMsg = GetErrorDescription(Watcher2NativeFlags(flags));
            wxFileSystemWatcherEvent event(flags, errMsg);
            SendEvent(event);
        }
        // filter out ignored events and those not asked for.
        // we never filter out warnings or exceptions
        else if ((flags == 0) || !(flags & watch.GetFlags()))
        {
            return;
        }
        // renames
        else if (nativeFlags & IN_MOVE)
        {
            wxInotifyCookies::iterator it = m_cookies.find(inevt.cookie);
            if ( it == m_cookies.end() )
            {
                int size = sizeof(inevt) + inevt.len;
                inotify_event* e = (inotify_event*) operator new (size);
                memcpy(e, &inevt, size);

                wxInotifyCookies::value_type val(e->cookie, e);
                m_cookies.insert(val);
            }
            else
            {
                inotify_event& oldinevt = *(it->second);

                wxFileSystemWatcherEvent event(flags);
                if ( inevt.mask & IN_MOVED_FROM )
                {
                    event.SetPath(GetEventPath(watch, inevt));
                    event.SetNewPath(GetEventPath(watch, oldinevt));
                }
                else
                {
                    event.SetPath(GetEventPath(watch, oldinevt));
                    event.SetNewPath(GetEventPath(watch, inevt));
                }
                SendEvent(event);

                m_cookies.erase(it);
                delete &oldinevt;
            }
        }
        // every other kind of event
        else
        {
            wxFileName path = GetEventPath(watch, inevt);
            wxFileSystemWatcherEvent event(flags, path, path);
            SendEvent(event);
        }
    }

    void ProcessRenames()
    {
        wxInotifyCookies::iterator it = m_cookies.begin();
        while ( it != m_cookies.end() )
        {
            inotify_event& inevt = *(it->second);

            wxLogTrace(wxTRACE_FSWATCHER, "Processing pending rename events");
            wxLogTrace(wxTRACE_FSWATCHER, InotifyEventToString(inevt));

            // get watch entry for this event
            wxFSWatchEntryDescriptors::iterator wit = m_watchMap.find(inevt.wd);
            wxCHECK_RET(wit != m_watchMap.end(),
                             "Watch descriptor not present in the watch map!");

            wxFSWatchEntry& watch = *(wit->second);
            int flags = Native2WatcherFlags(inevt.mask);
            wxFileName path = GetEventPath(watch, inevt);
            wxFileSystemWatcherEvent event(flags, path, path);
            SendEvent(event);

            m_cookies.erase(it);
            delete &inevt;
            it = m_cookies.begin();
        }
    }

    void SendEvent(wxFileSystemWatcherEvent& evt)
    {
        wxLogTrace(wxTRACE_FSWATCHER, evt.ToString());
        m_watcher->GetOwner()->ProcessEvent(evt);
    }

    int ReadEventsToBuf(char* buf, int size)
    {
        wxCHECK_MSG( IsOk(), false,
                    "Inotify not initialized or invalid inotify descriptor" );

        memset(buf, 0, size);
        ssize_t left = read(m_ifd, buf, size);
        if (left == -1)
        {
            wxLogSysError(_("Unable to read from inotify descriptor"));
            return -1;
        }
        else if (left == 0)
        {
            wxLogWarning(_("EOF while reading from inotify descriptor"));
            return -1;
        }

        return left;
    }

    static wxString InotifyEventToString(const inotify_event& inevt)
    {
        wxString mask = (inevt.mask & IN_ISDIR) ?
                        wxString::Format("IS_DIR | %u", inevt.mask & ~IN_ISDIR) :
                        wxString::Format("%u", inevt.mask);
        return wxString::Format("Event: wd=%d, mask=%s, cookie=%u, len=%u, "
                                "name=%s", inevt.wd, mask, inevt.cookie,
                                inevt.len, inevt.name);
    }

    static wxFileName GetEventPath(const wxFSWatchEntry& watch,
                                   const inotify_event& inevt)
    {
        // only when dir is watched, we have non-empty e.name
        wxFileName path = watch.GetPath();
        if (path.IsDir())
        {
            path = wxFileName(path.GetPath(), inevt.name);
        }
        return path;
    }

    static int Watcher2NativeFlags(int WXUNUSED(flags))
    {
        // TODO: it would be nice to subscribe only to the events we really need
        return IN_ALL_EVENTS;
    }

    static int Native2WatcherFlags(int flags)
    {
        static const int flag_mapping[][2] = {
            { IN_ACCESS,        wxFSW_EVENT_ACCESS }, // generated during read!
            { IN_MODIFY,        wxFSW_EVENT_MODIFY },
            { IN_ATTRIB,        0 },
            { IN_CLOSE_WRITE,   0 },
            { IN_CLOSE_NOWRITE, 0 },
            { IN_OPEN,          0 },
            { IN_MOVED_FROM,    wxFSW_EVENT_RENAME },
            { IN_MOVED_TO,      wxFSW_EVENT_RENAME },
            { IN_CREATE,        wxFSW_EVENT_CREATE },
            { IN_DELETE,        wxFSW_EVENT_DELETE },
            { IN_DELETE_SELF,   wxFSW_EVENT_DELETE },
            { IN_MOVE_SELF,     wxFSW_EVENT_DELETE },

            { IN_UNMOUNT,       wxFSW_EVENT_ERROR  },
            { IN_Q_OVERFLOW,    wxFSW_EVENT_WARNING},

            // ignored, because this is genereted mainly by watcher::Remove()
            { IN_IGNORED,        0 }
        };

        unsigned int i=0;
        for ( ; i < WXSIZEOF(flag_mapping); ++i) {
            // in this mapping multiple flags at once don't happen
            if (flags & flag_mapping[i][0])
                return flag_mapping[i][1];
        }

        // never reached
        wxFAIL_MSG(wxString::Format("Unknown inotify event mask %u", flags));
        return -1;
    }

    /**
     * Returns error description for specified inotify mask
     */
    static const wxString GetErrorDescription(int flag)
    {
        switch ( flag )
        {
        case IN_UNMOUNT:
            return _("File system containing watched object was unmounted");
        case IN_Q_OVERFLOW:
            return _("Event queue overflowed");
        }

        // never reached
        wxFAIL_MSG(wxString::Format("Unknown inotify event mask %u", flag));
        return wxEmptyString;
    }

    wxFSWSourceHandler* m_handler;        // handler for inotify event source
    wxFSWatchEntryDescriptors m_watchMap; // inotify wd=>wxFSWatchEntry* map
    wxInotifyCookies m_cookies;           // map to track renames
    wxEventLoopSource* m_source;          // our event loop source

    // file descriptor created by inotify_init()
    int m_ifd;
};


// ============================================================================
// wxFSWSourceHandler implementation
// ============================================================================

// once we get signaled to read, actuall event reading occurs
void wxFSWSourceHandler::OnReadWaiting()
{
    wxLogTrace(wxTRACE_FSWATCHER, "--- OnReadWaiting ---");
    m_service->ReadEvents();
}

void wxFSWSourceHandler::OnWriteWaiting()
{
    wxFAIL_MSG("We never write to inotify descriptor.");
}

void wxFSWSourceHandler::OnExceptionWaiting()
{
    wxFAIL_MSG("We never receive exceptions on inotify descriptor.");
}


// ============================================================================
// wxInotifyFileSystemWatcher implementation
// ============================================================================

wxInotifyFileSystemWatcher::wxInotifyFileSystemWatcher()
    : wxFileSystemWatcherBase()
{
    Init();
}

wxInotifyFileSystemWatcher::wxInotifyFileSystemWatcher(const wxFileName& path,
                                                       int events)
    : wxFileSystemWatcherBase()
{
    if (!Init())
    {
        if (m_service)
            delete m_service;
        return;
    }

    Add(path, events);
}

wxInotifyFileSystemWatcher::~wxInotifyFileSystemWatcher()
{
}

bool wxInotifyFileSystemWatcher::Init()
{
    m_service = new wxFSWatcherImplUnix(this);
    return m_service->Init();
}

#endif // wxHAS_INOTIFY

#endif // wxUSE_FSWATCHER
