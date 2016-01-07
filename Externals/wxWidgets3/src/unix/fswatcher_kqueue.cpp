/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/fswatcher_kqueue.cpp
// Purpose:     kqueue-based wxFileSystemWatcher implementation
// Author:      Bartosz Bekier
// Created:     2009-05-26
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

#ifdef wxHAS_KQUEUE

#include <sys/types.h>
#include <sys/event.h>

#include "wx/dynarray.h"
#include "wx/evtloop.h"
#include "wx/evtloopsrc.h"

#include "wx/private/fswatcher.h"

// ============================================================================
// wxFSWSourceHandler helper class
// ============================================================================

class wxFSWatcherImplKqueue;

/**
 * Handler for handling i/o from inotify descriptor
 */
class wxFSWSourceHandler : public wxEventLoopSourceHandler
{
public:
    wxFSWSourceHandler(wxFSWatcherImplKqueue* service) :
        m_service(service)
    {  }

    virtual void OnReadWaiting() wxOVERRIDE;
    virtual void OnWriteWaiting() wxOVERRIDE;
    virtual void OnExceptionWaiting() wxOVERRIDE;

protected:
    wxFSWatcherImplKqueue* m_service;
};

// ============================================================================
// wxFSWatcherImpl implementation & helper wxFSWSourceHandler implementation
// ============================================================================

/**
 * Helper class encapsulating inotify mechanism
 */
class wxFSWatcherImplKqueue : public wxFSWatcherImpl
{
public:
    wxFSWatcherImplKqueue(wxFileSystemWatcherBase* watcher) :
        wxFSWatcherImpl(watcher),
        m_source(NULL),
        m_kfd(-1)
    {
        m_handler = new wxFSWSourceHandler(this);
    }

    virtual ~wxFSWatcherImplKqueue()
    {
        // we close kqueue only if initialized before
        if (IsOk())
        {
            Close();
        }

        delete m_handler;
    }

    bool Init() wxOVERRIDE
    {
        wxCHECK_MSG( !IsOk(), false,
                     "Kqueue appears to be already initialized" );

        wxEventLoopBase *loop = wxEventLoopBase::GetActive();
        wxCHECK_MSG( loop, false, "File system watcher needs an active loop" );

        // create kqueue
        m_kfd = kqueue();
        if (m_kfd == -1)
        {
            wxLogSysError(_("Unable to create kqueue instance"));
            return false;
        }

        // create source
        m_source = loop->AddSourceForFD(m_kfd, m_handler, wxEVENT_SOURCE_INPUT);

        return m_source != NULL;
    }

    void Close()
    {
        wxCHECK_RET( IsOk(),
                    "Kqueue not initialized or invalid kqueue descriptor" );

        if ( close(m_kfd) != 0 )
        {
            wxLogSysError(_("Error closing kqueue instance"));
        }

        wxDELETE(m_source);
    }

    virtual bool DoAdd(wxSharedPtr<wxFSWatchEntryKq> watch) wxOVERRIDE
    {
        wxCHECK_MSG( IsOk(), false,
                    "Kqueue not initialized or invalid kqueue descriptor" );

        struct kevent event;
        int action = EV_ADD | EV_ENABLE | EV_CLEAR | EV_ERROR;
        int flags = Watcher2NativeFlags(watch->GetFlags());
        EV_SET( &event, watch->GetFileDescriptor(), EVFILT_VNODE, action,
                flags, 0, watch.get() );

        // TODO more error conditions according to man
        // TODO best deal with the error here
        int ret = kevent(m_kfd, &event, 1, NULL, 0, NULL);
        if (ret == -1)
        {
            wxLogSysError(_("Unable to add kqueue watch"));
            return false;
        }

        return true;
    }

    virtual bool DoRemove(wxSharedPtr<wxFSWatchEntryKq> watch) wxOVERRIDE
    {
        wxCHECK_MSG( IsOk(), false,
                    "Kqueue not initialized or invalid kqueue descriptor" );

        // TODO more error conditions according to man
        // XXX closing file descriptor removes the watch. The logic resides in
        // the watch which is not nice, but effective and simple
        if ( !watch->Close() )
        {
            wxLogSysError(_("Unable to remove kqueue watch"));
            return false;
        }

        return true;
    }

    virtual bool RemoveAll() wxOVERRIDE
    {
        wxFSWatchEntries::iterator it = m_watches.begin();
        for ( ; it != m_watches.end(); ++it )
        {
            (void) DoRemove(it->second);
        }
        m_watches.clear();
        return true;
    }

    // return true if there was no error, false on error
    bool ReadEvents()
    {
        wxCHECK_MSG( IsOk(), false,
                    "Kqueue not initialized or invalid kqueue descriptor" );

        // read events
        do
        {
            struct kevent event;
            struct timespec timeout = {0, 0};
            int ret = kevent(m_kfd, NULL, 0, &event, 1, &timeout);
            if (ret == -1)
            {
                wxLogSysError(_("Unable to get events from kqueue"));
                return false;
            }
            else if (ret == 0)
            {
                return true;
            }

            // we have event, so process it
            ProcessNativeEvent(event);
        }
        while (true);

        // when ret>0 we still have events, when ret<=0 we return
        wxFAIL_MSG( "Never reached" );
        return true;
    }

    bool IsOk() const
    {
        return m_source != NULL;
    }

protected:
    // returns all new dirs/files present in the immediate level of the dir
    // pointed by watch.GetPath(). "new" means created between the last time
    // the state of watch was computed and now
    void FindChanges(wxFSWatchEntryKq& watch,
                     wxArrayString& changedFiles,
                     wxArrayInt& changedFlags)
    {
        wxFSWatchEntryKq::wxDirState old = watch.GetLastState();
        watch.RefreshState();
        wxFSWatchEntryKq::wxDirState curr = watch.GetLastState();

        // iterate over old/curr file lists and compute changes
        wxArrayString::iterator oit = old.files.begin();
        wxArrayString::iterator cit = curr.files.begin();
        for ( ; oit != old.files.end() && cit != curr.files.end(); )
        {
            if ( *cit == *oit )
            {
                ++cit;
                ++oit;
            }
            else if ( *cit <= *oit )
            {
                changedFiles.push_back(*cit);
                changedFlags.push_back(wxFSW_EVENT_CREATE);
                ++cit;
            }
            else // ( *cit > *oit )
            {
                changedFiles.push_back(*oit);
                changedFlags.push_back(wxFSW_EVENT_DELETE);
                ++oit;
            }
        }

        // border conditions
        if ( oit == old.files.end() )
        {
            for ( ; cit != curr.files.end(); ++cit )
            {
                changedFiles.push_back( *cit );
                changedFlags.push_back(wxFSW_EVENT_CREATE);
            }
        }
        else if ( cit == curr.files.end() )
        {
            for ( ; oit != old.files.end(); ++oit )
            {
                changedFiles.push_back( *oit );
                changedFlags.push_back(wxFSW_EVENT_DELETE);
            }
        }

        wxASSERT( changedFiles.size() == changedFlags.size() );

#if 0
        wxLogTrace(wxTRACE_FSWATCHER, "Changed files:");
        wxArrayString::iterator it = changedFiles.begin();
        wxArrayInt::iterator it2 = changedFlags.begin();
        for ( ; it != changedFiles.end(); ++it, ++it2)
        {
            wxString action = (*it2 == wxFSW_EVENT_CREATE) ?
                                "created" : "deleted";
            wxLogTrace(wxTRACE_FSWATCHER, wxString::Format("File: '%s' %s",
                        *it, action));
        }
#endif
    }

    void ProcessNativeEvent(const struct kevent& e)
    {
        wxASSERT_MSG(e.udata, "Null user data associated with kevent!");

        wxLogTrace(wxTRACE_FSWATCHER, "Event: ident=%llu, filter=%d, flags=%u, "
                   "fflags=%u, data=%lld, user_data=%lp",
                   e.ident, e.filter, e.flags, e.fflags, e.data, e.udata);

        // for ease of use
        wxFSWatchEntryKq& w = *(static_cast<wxFSWatchEntry*>(e.udata));
        int nflags = e.fflags;

        // clear ignored flags
        nflags &= ~NOTE_REVOKE;

        // TODO ignore events we didn't ask for + refactor this cascade ifs
        // check for events
        while ( nflags )
        {
            // when monitoring dir, this means create/delete
            const wxString basepath = w.GetPath();
            if ( nflags & NOTE_WRITE && wxDirExists(basepath) )
            {
                // NOTE_LINK is set when the dir was created, but we
                // don't care - we look for new names in directory
                // regardless of type. Also, clear all this, because
                // it cannot mean more by itself
                nflags &= ~(NOTE_WRITE | NOTE_ATTRIB | NOTE_LINK);

                wxArrayString changedFiles;
                wxArrayInt changedFlags;
                FindChanges(w, changedFiles, changedFlags);

                wxArrayString::iterator it = changedFiles.begin();
                wxArrayInt::iterator changeType = changedFlags.begin();
                for ( ; it != changedFiles.end(); ++it, ++changeType )
                {
                    const wxString fullpath = w.GetPath() +
                                                wxFileName::GetPathSeparator() +
                                                  *it;
                    const wxFileName path(wxDirExists(fullpath)
                                            ? wxFileName::DirName(fullpath)
                                            : wxFileName::FileName(fullpath));

                    wxFileSystemWatcherEvent event(*changeType, path, path);
                    SendEvent(event);
                }
            }
            else if ( nflags & NOTE_RENAME )
            {
                // CHECK it'd be only possible to detect name if we had
                // parent files listing which we could confront with now and
                // still we couldn't be sure we have the right name...
                nflags &= ~NOTE_RENAME;
                wxFileSystemWatcherEvent event(wxFSW_EVENT_RENAME,
                                        basepath, wxFileName());
                SendEvent(event);
            }
            else if ( nflags & NOTE_WRITE || nflags & NOTE_EXTEND )
            {
                nflags &= ~(NOTE_WRITE | NOTE_EXTEND);
                wxFileSystemWatcherEvent event(wxFSW_EVENT_MODIFY,
                                        basepath, basepath);
                SendEvent(event);
            }
            else if ( nflags & NOTE_DELETE )
            {
                nflags &= ~(NOTE_DELETE);
                wxFileSystemWatcherEvent event(wxFSW_EVENT_DELETE,
                                        basepath, basepath);
                SendEvent(event);
            }
            else if ( nflags & NOTE_ATTRIB )
            {
                nflags &= ~(NOTE_ATTRIB);
                wxFileSystemWatcherEvent event(wxFSW_EVENT_ACCESS,
                                        basepath, basepath);
                SendEvent(event);
            }

            // clear any flags that won't mean anything by themselves
            nflags &= ~(NOTE_LINK);
        }
    }

    void SendEvent(wxFileSystemWatcherEvent& evt)
    {
        m_watcher->GetOwner()->ProcessEvent(evt);
    }

    static int Watcher2NativeFlags(int WXUNUSED(flags))
    {
        // TODO: it would be better to only subscribe to what we need
        return NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND |
               NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME |
               NOTE_REVOKE;
    }

    wxFSWSourceHandler* m_handler;        // handler for kqueue event source
    wxEventLoopSource* m_source;          // our event loop source

    // descriptor created by kqueue()
    int m_kfd;
};


// once we get signaled to read, actuall event reading occurs
void wxFSWSourceHandler::OnReadWaiting()
{
    wxLogTrace(wxTRACE_FSWATCHER, "--- OnReadWaiting ---");
    m_service->ReadEvents();
}

void wxFSWSourceHandler::OnWriteWaiting()
{
    wxFAIL_MSG("We never write to kqueue descriptor.");
}

void wxFSWSourceHandler::OnExceptionWaiting()
{
    wxFAIL_MSG("We never receive exceptions on kqueue descriptor.");
}


// ============================================================================
// wxKqueueFileSystemWatcher implementation
// ============================================================================

wxKqueueFileSystemWatcher::wxKqueueFileSystemWatcher()
    : wxFileSystemWatcherBase()
{
    Init();
}

wxKqueueFileSystemWatcher::wxKqueueFileSystemWatcher(const wxFileName& path,
                                                     int events)
    : wxFileSystemWatcherBase()
{
    if (!Init())
    {
        wxDELETE(m_service);
        return;
    }

    Add(path, events);
}

wxKqueueFileSystemWatcher::~wxKqueueFileSystemWatcher()
{
}

bool wxKqueueFileSystemWatcher::Init()
{
    m_service = new wxFSWatcherImplKqueue(this);
    return m_service->Init();
}

#endif // wxHAS_KQUEUE

#endif // wxUSE_FSWATCHER
