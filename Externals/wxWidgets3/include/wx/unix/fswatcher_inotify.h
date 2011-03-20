/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/fswatcher_inotify.h
// Purpose:     wxInotifyFileSystemWatcher
// Author:      Bartosz Bekier
// Created:     2009-05-26
// RCS-ID:      $Id: fswatcher_inotify.h 62474 2009-10-22 11:35:43Z VZ $
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FSWATCHER_UNIX_H_
#define _WX_FSWATCHER_UNIX_H_

#include "wx/defs.h"

#if wxUSE_FSWATCHER

class WXDLLIMPEXP_BASE wxInotifyFileSystemWatcher :
        public wxFileSystemWatcherBase
{
public:
    wxInotifyFileSystemWatcher();

    wxInotifyFileSystemWatcher(const wxFileName& path,
                               int events = wxFSW_EVENT_ALL);

    virtual ~wxInotifyFileSystemWatcher();

protected:
    bool Init();
};

#endif

#endif /* _WX_FSWATCHER_UNIX_H_ */
