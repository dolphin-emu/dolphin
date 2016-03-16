/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/fswatcher_fsevents.cpp
// Purpose:     File System watcher that uses the FSEvents API
//              of OS X to efficiently watch trees
// Author:      Roberto Perpuly
// Created:     2015-04-24
// Copyright:   (c) 2015 Roberto Perpuly <robertop2004@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FSWATCHER && defined(wxHAVE_FSEVENTS_FILE_NOTIFICATIONS)

#include "wx/fswatcher.h"
#include "wx/osx/core/cfstring.h"
#include <CoreFoundation/CoreFoundation.h>

// A small class that we will give the FSEvents
// framework, which will be forwarded to the function
// that gets called when files change.
class wxFSEventWatcherContext
{
public:

    // Watcher pointer will not be owned by this class.
    wxFSEventWatcherContext(wxFsEventsFileSystemWatcher* watcher,
    int watcherEventFlags,
    const wxString& filespec)
    : m_watcher(watcher)
    , m_watcherEventFlags(watcherEventFlags)
    , m_filespec(filespec)
    {

    }

    // Will return true if the given event file and flags
    // match the filespec and event flags given to the
    // AddTree method.
    bool IsDesiredEvent(const wxFileName& eventFileName, int eventFlags)
    {
        // warning and errors are always sent to the event handler
        if ( eventFlags & wxFSW_EVENT_ERROR )
        {
            return true;
        }
        if ( eventFlags & wxFSW_EVENT_WARNING )
        {
            return true;
        }

        if ( (m_watcherEventFlags & eventFlags) == 0 )
        {
        // event handler does not want to see this event
            return false;
        }

        return m_filespec.empty() ||
            wxMatchWild(m_filespec, eventFileName.GetFullName());
    }

    wxFsEventsFileSystemWatcher* m_watcher;

    // The event flags that the event handler
    // desires to be notified of.
    int m_watcherEventFlags;

    // The filespec that the event handler
    // desires to be notified of.
    wxString m_filespec;

private:

    wxDECLARE_NO_COPY_CLASS(wxFSEventWatcherContext);
};

// Translate kFSEventStreamEventFlag* flags
// to wxFSW_EVENT_* flags.
// warning and msg are out parameters, filled in when
// there is an error in the stream.
static int wxFSEventsToWatcherFlags(FSEventStreamEventFlags flags,
wxFSWWarningType& warning, wxString& msg)
{
    msg = "";
    warning = wxFSW_WARNING_NONE;

    // see https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/index.html
    // for event flag meanings
    int ret = 0;
    int warnings =
        kFSEventStreamEventFlagMustScanSubDirs
        | kFSEventStreamEventFlagUserDropped
        | kFSEventStreamEventFlagKernelDropped
        | kFSEventStreamEventFlagMount
    ;

    int errors = kFSEventStreamEventFlagRootChanged;

    // The following flags are not handled:
    // kFSEventStreamEventFlagHistoryDone (we never ask for old events)
    // kFSEventStreamEventFlagEventIdsWrapped ( we don't keep track nor
    //  expose event IDs)

    int created = kFSEventStreamEventFlagItemCreated;
    int deleted = kFSEventStreamEventFlagItemRemoved;
    int renamed = kFSEventStreamEventFlagItemRenamed;
    int modified = kFSEventStreamEventFlagItemModified;
    int attrib = kFSEventStreamEventFlagItemChangeOwner
        | kFSEventStreamEventFlagItemFinderInfoMod
        | kFSEventStreamEventFlagItemInodeMetaMod
        | kFSEventStreamEventFlagItemXattrMod;

    if ( created & flags )
    {
        ret |= wxFSW_EVENT_CREATE;
    }
    if ( deleted & flags )
    {
        ret |= wxFSW_EVENT_DELETE;
    }
    if ( renamed & flags )
    {
        ret |= wxFSW_EVENT_RENAME;
    }
    if ( modified & flags )
    {
        ret |= wxFSW_EVENT_MODIFY;
    }
    if ( attrib & flags )
    {
        ret |= wxFSW_EVENT_ATTRIB;
    }
    if ( kFSEventStreamEventFlagUnmount & flags )
    {
        ret |= wxFSW_EVENT_UNMOUNT;
    }
    if ( warnings & flags )
    {
        warning = wxFSW_WARNING_GENERAL;
        ret |= wxFSW_EVENT_WARNING;
        if (flags & kFSEventStreamEventFlagMustScanSubDirs)
        {
            msg += "Must re-scan sub directories.";
        }
        if (flags & kFSEventStreamEventFlagUserDropped)
        {
            msg += "User dropped events";
            warning = wxFSW_WARNING_OVERFLOW;
        }
        if (flags & kFSEventStreamEventFlagKernelDropped)
        {
            msg += "Kernel dropped events";
            warning = wxFSW_WARNING_OVERFLOW;
        }
        if (flags & kFSEventStreamEventFlagMount)
        {
            msg += "A volume was mounted underneath the watched directory.";
        }
    }
    if ( errors & flags )
    {
        ret |= wxFSW_EVENT_ERROR;
        msg = "Path being watched has been renamed";
    }

    //  don't think that FSEvents tells us about wxFSW_EVENT_ACCESS
    return ret;
}

// Fills in eventFileName appropriately based on whether the
// event was on a file or a directory.
static void FileNameFromEvent(wxFileName& eventFileName, char* path,
    FSEventStreamEventFlags flags)
{
    wxString strPath(path);
    if ( flags & kFSEventStreamEventFlagItemIsFile )
    {
        eventFileName.Assign(strPath);
    }
    if ( flags & kFSEventStreamEventFlagItemIsDir )
    {
        eventFileName.AssignDir(strPath);
    }
}

// This is the function that the FsEvents API
// will call to notify us that a file has been changed.
static void wxFSEventCallback(ConstFSEventStreamRef WXUNUSED(streamRef), void *clientCallBackInfo,
    size_t numEvents, void *eventPaths, const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId WXUNUSED(eventIds)[])
{
    wxFSEventWatcherContext* context =
        (wxFSEventWatcherContext*) clientCallBackInfo;

    char** paths = (char**) eventPaths;
    int lastWxEventFlags = 0;
    wxFileName lastEventFileName;
    wxString msg;
    wxFSWWarningType warning = wxFSW_WARNING_NONE;
    wxFileName eventFileName;
    for ( size_t i = 0; i < numEvents; i++ )
    {
        FSEventStreamEventFlags flags = eventFlags[i];
        FileNameFromEvent(eventFileName, paths[i], flags);
        int wxEventFlags = wxFSEventsToWatcherFlags(flags, warning, msg);
        if ( context->IsDesiredEvent(eventFileName, wxEventFlags) )
        {
            // This is a naive way of looking for file renames
            // wx presents a renames with a from and to paths
            // but fs events events do not give us this (it only
            // provides that a file was renamed, not what the new
            // name is).
            // We deduce the old and new paths by looking for consecutive
            // renames. This is very naive and won't catch simulatenous
            // renames inside the latency period, nor renames from/to
            // a directory that is not inside the watched paths.
            if (wxEventFlags == wxFSW_EVENT_RENAME && lastWxEventFlags == wxFSW_EVENT_RENAME)
            {
                context->m_watcher->PostChange(lastEventFileName, eventFileName, wxEventFlags);
            }
            else if (flags == kFSEventStreamEventFlagRootChanged)
            {
                // send two events: the delete event and the error event
                context->m_watcher->PostChange(eventFileName, eventFileName, wxFSW_EVENT_DELETE);
                context->m_watcher->PostError(msg);
            }
            else if (wxEventFlags != wxFSW_EVENT_RENAME)
            {
                context->m_watcher->PostChange(eventFileName, eventFileName, wxEventFlags);
            }
            else
            {
                // This is a "rename" event that we only saw once, meaning that
                // a file was renamed to somewhere inside the watched tree
                // OR a file was renamed to somewhere outside the watched tree.
                if (!eventFileName.IsDir())
                {
                    int fileEventType = eventFileName.FileExists() ? wxFSW_EVENT_CREATE : wxFSW_EVENT_DELETE;
                    context->m_watcher->PostChange(eventFileName, eventFileName, fileEventType);
                }
                if (eventFileName.IsDir())
                {
                    int dirEventType = eventFileName.DirExists() ? wxFSW_EVENT_CREATE : wxFSW_EVENT_DELETE;
                    context->m_watcher->PostChange(eventFileName, eventFileName, dirEventType);
                }
            }

            if (wxEventFlags & wxFSW_EVENT_WARNING)
            {
                context->m_watcher->PostWarning(warning, msg);
            }

            // A single rename (without the second rename) may be due
            // to the file being renamed into a directory outside of the
            // watch path.
            lastWxEventFlags = wxEventFlags;
            lastEventFileName = eventFileName;
        }
    }
}

static void wxDeleteContext(const void* context)
{
    wxFSEventWatcherContext* watcherContext =
        (wxFSEventWatcherContext*) context;
    delete watcherContext;
}

wxFsEventsFileSystemWatcher::wxFsEventsFileSystemWatcher()
: wxKqueueFileSystemWatcher()
{

}

wxFsEventsFileSystemWatcher::wxFsEventsFileSystemWatcher(const wxFileName& path,
    int events)
: wxKqueueFileSystemWatcher(path, events)
{

}

wxFsEventsFileSystemWatcher::~wxFsEventsFileSystemWatcher()
{

}

bool wxFsEventsFileSystemWatcher::AddTree(const wxFileName& path, int events,
    const wxString& filespec)
{
    if (!path.DirExists())
    {
        return false;
    }
    wxString canonical = GetCanonicalPath(path);
    if ( canonical.empty() )
    {
        return false;
    }
    CFRunLoopRef cfLoop = CFRunLoopGetCurrent();
    wxASSERT_MSG(
        cfLoop,
        "there must be a current event loop; this file watcher needs it."
    );
    if ( ! cfLoop )
    {
        return false;
    }

    if ( m_streams.find(canonical) != m_streams.end() )
    {
        // How to take into account filespec
        // if client adds a watch for /home/*.cpp
        // and then on another call wants to add a
        // call to /home/*.h
        // Ideally we should not create another watch
        // however we would need to keep both filespecs
        // around, which we don't do now.
        return false;
    }

    // Will need to pass the desired event flags
    // and filespec to our callback via the context
    // we make sure to give the context a cleanup
    // callback.
    FSEventStreamContext ctx;
    wxFSEventWatcherContext* watcherContext = new wxFSEventWatcherContext(
        this, events, filespec.Clone()
    );
    ctx.version = 0;
    ctx.info = watcherContext;
    ctx.retain = NULL;
    ctx.release = &wxDeleteContext;
    ctx.copyDescription = NULL;
    CFTimeInterval latency = 0.2;

    wxMacUniCharBuffer pathChars(path.GetPath());
    CFStringRef pathRef = CFStringCreateWithCharacters(
        kCFAllocatorDefault,
        pathChars.GetBuffer(),
        pathChars.GetChars()
    );
    CFArrayRef pathRefs = CFArrayCreate(
        kCFAllocatorDefault, (const void**)&pathRef, 1, NULL
    );
    FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagWatchRoot
        | kFSEventStreamCreateFlagFileEvents;

    FSEventStreamRef stream = FSEventStreamCreate(
        kCFAllocatorDefault,
        &wxFSEventCallback,
        &ctx,
        pathRefs, kFSEventStreamEventIdSinceNow,
        latency, flags);
    bool started = false;
    if ( stream )
    {
        FSEventStreamScheduleWithRunLoop(stream, cfLoop, kCFRunLoopDefaultMode);
        started = FSEventStreamStart(stream);
        if ( started )
        {
            m_streams[canonical] = stream;
        }
    }

    // cleanup the paths, as we own the pointers
    CFRelease(pathRef);
    CFRelease(pathRefs);

    wxASSERT_MSG(stream, "could not create FS stream");
    return started;
}

bool wxFsEventsFileSystemWatcher::RemoveTree(const wxFileName& path)
{
    wxString canonical = GetCanonicalPath(path);
    if ( canonical.empty() )
    {
        return false;
    }

    // Remove any kqueue watches created with Add()
    // RemoveTree() should remove all watches no matter
    // if they are tree watches or single directory watches.
    wxArrayString dirsWatched;
    wxKqueueFileSystemWatcher::GetWatchedPaths(&dirsWatched);
    for ( size_t i = 0; i < dirsWatched.size(); i++ )
    {
        if (dirsWatched[i].Find(canonical) == 0)
        {
            wxKqueueFileSystemWatcher::Remove(dirsWatched[i]);
        }
    }

    FSEventStreamRefMap::iterator it = m_streams.find(canonical);
    bool removed = false;
    if ( it != m_streams.end() )
    {
        FSEventStreamStop(it->second);
        FSEventStreamInvalidate(it->second);
        FSEventStreamRelease(it->second);
        m_streams.erase(it);
        removed = true;
    }
    return removed;
}

bool wxFsEventsFileSystemWatcher::RemoveAll()
{
    // remove all watches created with Add()
    bool ret = wxKqueueFileSystemWatcher::RemoveAll();
    FSEventStreamRefMap::iterator it = m_streams.begin();
    while ( it != m_streams.end() )
    {
        FSEventStreamStop(it->second);
        FSEventStreamInvalidate(it->second);
        FSEventStreamRelease(it->second);
        it++;
        ret |= true;
    }
    m_streams.clear();
    return ret;
}

void wxFsEventsFileSystemWatcher::PostChange(const wxFileName& oldFileName,
    const wxFileName& newFileName, int event)
{
    wxASSERT_MSG(this->GetOwner(), "owner must exist");
    if ( !this->GetOwner() )
    {
        return;
    }

    // FSEvents flags are a bit mask, but wx FSW events
    // are not, meaning that FSEvent flag might be
    // kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemInodeMetaMod
    // this means we must send 2 events not 1.
    int allEvents[6] = {
        wxFSW_EVENT_CREATE,
        wxFSW_EVENT_DELETE,
        wxFSW_EVENT_RENAME,
        wxFSW_EVENT_MODIFY,
        wxFSW_EVENT_ACCESS,
        wxFSW_EVENT_ATTRIB
    };

    for ( int i = 0; i < WXSIZEOF(allEvents); i++ )
    {
        if ( event & allEvents[i] )
        {
            wxFileSystemWatcherEvent* evt = new wxFileSystemWatcherEvent(
                allEvents[i], oldFileName, newFileName
            );
            wxQueueEvent(this->GetOwner(), evt);
        }
    }
}

void wxFsEventsFileSystemWatcher::PostWarning(wxFSWWarningType warning,
    const wxString& msg)
{
    wxFileSystemWatcherEvent* evt = new wxFileSystemWatcherEvent(
        wxFSW_EVENT_WARNING, warning, msg
    );
    wxASSERT_MSG(this->GetOwner(), "owner must exist");
    if (this->GetOwner())
    {
        wxQueueEvent(this->GetOwner(), evt);
    }
}

void wxFsEventsFileSystemWatcher::PostError(const wxString& msg)
{
    wxFileSystemWatcherEvent* evt = new wxFileSystemWatcherEvent(
        wxFSW_EVENT_ERROR, wxFSW_WARNING_NONE, msg
    );
    wxASSERT_MSG(this->GetOwner(), "owner must exist");
    if (this->GetOwner())
    {
        wxQueueEvent(this->GetOwner(), evt);
    }
}

int wxFsEventsFileSystemWatcher::GetWatchedPathsCount() const
{
    return m_streams.size() + wxFileSystemWatcherBase::GetWatchedPathsCount();
}

int wxFsEventsFileSystemWatcher::GetWatchedPaths(wxArrayString* paths) const
{
    wxCHECK_MSG( paths != NULL, -1, "Null array passed to retrieve paths");
    if ( !paths )
    {
        return 0;
    }
    wxFileSystemWatcherBase::GetWatchedPaths(paths);
    FSEventStreamRefMap::const_iterator it = m_streams.begin();
    for ( ; it != m_streams.end(); it++ )
    {
        paths->push_back(it->first);
    }
    return paths->size();
}

#endif // wxUSE_FSWATCHER
