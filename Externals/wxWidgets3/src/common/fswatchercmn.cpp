/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fswatchercmn.cpp
// Purpose:     wxMswFileSystemWatcher
// Author:      Bartosz Bekier
// Created:     2009-05-26
// RCS-ID:      $Id: fswatchercmn.cpp 67693 2011-05-03 23:31:39Z VZ $
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
#include "wx/private/fswatcher.h"

// ============================================================================
// helpers
// ============================================================================

wxDEFINE_EVENT(wxEVT_FSWATCHER, wxFileSystemWatcherEvent);

static wxString GetFSWEventChangeTypeName(int type)
{
    switch (type)
    {
    case wxFSW_EVENT_CREATE:
        return "CREATE";
    case wxFSW_EVENT_DELETE:
        return "DELETE";
    case wxFSW_EVENT_RENAME:
        return "RENAME";
    case wxFSW_EVENT_MODIFY:
        return "MODIFY";
    case wxFSW_EVENT_ACCESS:
        return "ACCESS";
    }

    // should never be reached!
    wxFAIL_MSG("Unknown change type");
    return "INVALID_TYPE";
}


// ============================================================================
// wxFileSystemWatcherEvent implementation
// ============================================================================

wxString wxFileSystemWatcherEvent::ToString() const
{
    return wxString::Format("FSW_EVT type=%d (%s) path='%s'", m_changeType,
            GetFSWEventChangeTypeName(m_changeType), GetPath().GetFullPath());
}


// ============================================================================
// wxFileSystemWatcherEvent implementation
// ============================================================================

wxFileSystemWatcherBase::wxFileSystemWatcherBase() :
    m_service(0), m_owner(this)
{
}

wxFileSystemWatcherBase::~wxFileSystemWatcherBase()
{
    RemoveAll();
    if (m_service)
    {
        delete m_service;
    }
}

bool wxFileSystemWatcherBase::Add(const wxFileName& path, int events)
{
    wxFSWPathType type = wxFSWPath_None;
    if ( path.FileExists() )
    {
        type = wxFSWPath_File;
    }
    else if ( path.DirExists() )
    {
        type = wxFSWPath_Dir;
    }
    else
    {
        wxLogError(_("Can't monitor non-existent path \"%s\" for changes."),
                   path.GetFullPath());
        return false;
    }

    return DoAdd(path, events, type);
}

bool
wxFileSystemWatcherBase::DoAdd(const wxFileName& path,
                               int events,
                               wxFSWPathType type)
{
    wxString canonical = GetCanonicalPath(path);
    if (canonical.IsEmpty())
        return false;

    wxCHECK_MSG(m_watches.find(canonical) == m_watches.end(), false,
                wxString::Format("Path '%s' is already watched", canonical));

    // adding a path in a platform specific way
    wxFSWatchInfo watch(canonical, events, type);
    if ( !m_service->Add(watch) )
        return false;

    // on success, add path to our 'watch-list'
    wxFSWatchInfoMap::value_type val(canonical, watch);
    return m_watches.insert(val).second;
}

bool wxFileSystemWatcherBase::Remove(const wxFileName& path)
{
    // args validation & consistency checks
    wxString canonical = GetCanonicalPath(path);
    if (canonical.IsEmpty())
        return false;

    wxFSWatchInfoMap::iterator it = m_watches.find(canonical);
    wxCHECK_MSG(it != m_watches.end(), false,
                wxString::Format("Path '%s' is not watched", canonical));

    // remove from watch-list
    wxFSWatchInfo watch = it->second;
    m_watches.erase(it);

    // remove in a platform specific way
    return m_service->Remove(watch);
}

bool wxFileSystemWatcherBase::AddTree(const wxFileName& path, int events,
                                      const wxString& filter)
{
    if (!path.DirExists())
        return false;

    // OPT could be optimised if we stored information about relationships
    // between paths
    class AddTraverser : public wxDirTraverser
    {
    public:
        AddTraverser(wxFileSystemWatcherBase* watcher, int events) :
            m_watcher(watcher), m_events(events)
        {
        }

        // CHECK we choose which files to delegate to Add(), maybe we should pass
        // all of them to Add() and let it choose? this is useful when adding a
        // file to a dir that is already watched, then not only should we know
        // about that, but Add() should also behave well then
        virtual wxDirTraverseResult OnFile(const wxString& WXUNUSED(filename))
        {
            return wxDIR_CONTINUE;
        }

        virtual wxDirTraverseResult OnDir(const wxString& dirname)
        {
            wxLogTrace(wxTRACE_FSWATCHER, "--- AddTree adding '%s' ---",
                                                                    dirname);
            // we add as much as possible and ignore errors
            m_watcher->Add(wxFileName(dirname), m_events);
            return wxDIR_CONTINUE;
        }

    private:
        wxFileSystemWatcherBase* m_watcher;
        int m_events;
        wxString m_filter;
    };

    wxDir dir(path.GetFullPath());
    AddTraverser traverser(this, events);
    dir.Traverse(traverser, filter);

    return true;
}

bool wxFileSystemWatcherBase::RemoveTree(const wxFileName& path)
{
    if (!path.DirExists())
        return false;

    // OPT could be optimised if we stored information about relationships
    // between paths
    class RemoveTraverser : public wxDirTraverser
    {
    public:
        RemoveTraverser(wxFileSystemWatcherBase* watcher) :
            m_watcher(watcher)
        {
        }

        virtual wxDirTraverseResult OnFile(const wxString& filename)
        {
            m_watcher->Remove(wxFileName(filename));
            return wxDIR_CONTINUE;
        }

        virtual wxDirTraverseResult OnDir(const wxString& dirname)
        {
            m_watcher->RemoveTree(wxFileName(dirname));
            return wxDIR_CONTINUE;
        }

    private:
        wxFileSystemWatcherBase* m_watcher;
    };

    wxDir dir(path.GetFullPath());
    RemoveTraverser traverser(this);
    dir.Traverse(traverser);

    return true;
}

bool wxFileSystemWatcherBase::RemoveAll()
{
    m_service->RemoveAll();
    m_watches.clear();
    return true;
}

int wxFileSystemWatcherBase::GetWatchedPathsCount() const
{
    return m_watches.size();
}

int wxFileSystemWatcherBase::GetWatchedPaths(wxArrayString* paths) const
{
    wxCHECK_MSG( paths != NULL, -1, "Null array passed to retrieve paths");

    wxFSWatchInfoMap::const_iterator it = m_watches.begin();
    for ( ; it != m_watches.end(); ++it)
    {
        paths->push_back(it->first);
    }

    return m_watches.size();
}

#endif // wxUSE_FSWATCHER
