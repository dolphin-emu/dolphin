/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/fswatcher_fsevents.h
// Purpose:     File System watcher that uses the FSEvents API
//              of OS X to efficiently watch trees
// Author:      Roberto Perpuly
// Created:     2015-04-24
// Copyright:   (c) 2015 Roberto Perpuly <robertop2004@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FSWATCHER_FSEVENTS_H_
#define _WX_FSWATCHER_FSEVENTS_H_

#include "wx/defs.h"

#if wxUSE_FSWATCHER

#include <CoreServices/CoreServices.h>
#include "wx/unix/fswatcher_kqueue.h"

WX_DECLARE_STRING_HASH_MAP(FSEventStreamRef, FSEventStreamRefMap);

/*
 The FSEvents watcher uses the newer FSEvents service
 that is available in OS X, the service allows for
 efficient watching of entire directory hierarchies.
 Note that adding a single file watch (or directory
 watch) still use kqueue events.

 We take care to only use this on OS X >= 10.7, as that
 version introduced the ability to get file-level notifications.

 See the following docs that outline the FSEvents API

 https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/FSEvents_ProgGuide/UsingtheFSEventsFramework/UsingtheFSEventsFramework.html

 https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/index.html
*/
class WXDLLIMPEXP_BASE wxFsEventsFileSystemWatcher :
        public wxKqueueFileSystemWatcher
{
public:
    wxFsEventsFileSystemWatcher();

    wxFsEventsFileSystemWatcher(const wxFileName& path,
                              int events = wxFSW_EVENT_ALL);

    ~wxFsEventsFileSystemWatcher();

    // reimplement adding a tree so that it does not use
    // kqueue at all
    bool AddTree(const wxFileName& path, int events = wxFSW_EVENT_ALL,
                const wxString& filespec = wxEmptyString) wxOVERRIDE;

    // reimplement removing a tree so that we
    // cleanup the opened fs streams
    bool RemoveTree(const wxFileName& path) wxOVERRIDE;

    // reimplement remove all so that we cleanup
    // watches from kqeueue and from FSEvents
    bool RemoveAll() wxOVERRIDE;

    // post an file change event to the owner
    void PostChange(const wxFileName& oldFileName,
      const wxFileName& newFileName, int event);

    // post a warning event to the owner
    void PostWarning(wxFSWWarningType warning, const wxString& msg);

    // post an error event to the owner
    void PostError(const wxString& msg);

    // reimplement count to include the FS stream watches
    int GetWatchedPathsCount() const;

    // reimplement to include paths from FS stream watches
    int GetWatchedPaths(wxArrayString* paths) const;

private:

    // map of path => FSEventStreamRef
    FSEventStreamRefMap m_streams;

};

#endif /* wxUSE_FSWATCHER */

#endif  /* _WX_FSWATCHER_FSEVENTS_H_ */
