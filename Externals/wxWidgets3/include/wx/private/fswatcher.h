/////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/fswatcher.h
// Purpose:     File system watcher impl classes
// Author:      Bartosz Bekier
// Created:     2009-05-26
// RCS-ID:      $Id: fswatcher.h 62475 2009-10-22 11:36:35Z VZ $
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef WX_PRIVATE_FSWATCHER_H_
#define WX_PRIVATE_FSWATCHER_H_

#include "wx/sharedptr.h"

#ifdef wxHAS_INOTIFY
    class wxFSWatchEntryUnix;
    #define wxFSWatchEntry wxFSWatchEntryUnix
    WX_DECLARE_STRING_HASH_MAP(wxSharedPtr<wxFSWatchEntry>,wxFSWatchEntries);
    #include "wx/unix/private/fswatcher_inotify.h"
#elif defined(wxHAS_KQUEUE)
    class wxFSWatchEntryKq;
    #define wxFSWatchEntry wxFSWatchEntryKq
    WX_DECLARE_STRING_HASH_MAP(wxSharedPtr<wxFSWatchEntry>,wxFSWatchEntries);
    #include "wx/unix/private/fswatcher_kqueue.h"
#elif defined(__WXMSW__)
    class wxFSWatchEntryMSW;
    #define wxFSWatchEntry wxFSWatchEntryMSW
    WX_DECLARE_STRING_HASH_MAP(wxSharedPtr<wxFSWatchEntry>,wxFSWatchEntries);
    #include "wx/msw/private/fswatcher.h"
#else
    #define wxFSWatchEntry wxFSWatchEntryPolling
#endif

class wxFSWatcherImpl
{
public:
    wxFSWatcherImpl(wxFileSystemWatcherBase* watcher) :
        m_watcher(watcher)
    {
    }

    virtual ~wxFSWatcherImpl()
    {
        (void) RemoveAll();
    }

    virtual bool Init() = 0;

    virtual bool Add(const wxFSWatchInfo& winfo)
    {
        wxCHECK_MSG( m_watches.find(winfo.GetPath()) == m_watches.end(), false,
                     "Path '%s' is already watched");

        // construct watch entry
        wxSharedPtr<wxFSWatchEntry> watch(new wxFSWatchEntry(winfo));

        if (!DoAdd(watch))
            return false;

        // add watch to our map (always succeedes, checked above)
        wxFSWatchEntries::value_type val(watch->GetPath(), watch);
        return m_watches.insert(val).second;
    }

    virtual bool Remove(const wxFSWatchInfo& winfo)
    {
        wxFSWatchEntries::iterator it = m_watches.find(winfo.GetPath());
        wxCHECK_MSG( it != m_watches.end(), false, "Path '%s' is not watched");

        wxSharedPtr<wxFSWatchEntry> watch = it->second;
        m_watches.erase(it);
        return DoRemove(watch);
    }

    virtual bool RemoveAll()
    {
        m_watches.clear();
        return true;
    }

protected:
    virtual bool DoAdd(wxSharedPtr<wxFSWatchEntry> watch) = 0;

    virtual bool DoRemove(wxSharedPtr<wxFSWatchEntry> watch) = 0;

    wxFSWatchEntries m_watches;
    wxFileSystemWatcherBase* m_watcher;
};


#endif /* WX_PRIVATE_FSWATCHER_H_ */
