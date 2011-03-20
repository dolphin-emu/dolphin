///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/threadinfo.h
// Purpose:     declaration of wxThreadSpecificInfo: thread-specific information
// Author:      Vadim Zeitlin
// Created:     2009-07-13
// RCS-ID:      $Id: threadinfo.h 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_THREADINFO_H_
#define _WX_PRIVATE_THREADINFO_H_

#if wxUSE_THREADS

#include "wx/tls.h"

class WXDLLIMPEXP_FWD_BASE wxLog;

// ----------------------------------------------------------------------------
// wxThreadSpecificInfo: contains all thread-specific information used by wx
// ----------------------------------------------------------------------------

// currently the only thread-specific information we use is the active wxLog
// target but more could be added in the future (e.g. current wxLocale would be
// a likely candidate) and we will group all of them in this struct to avoid
// consuming more TLS slots than necessary as there is only a limited number of
// them

// NB: this must be a POD to be stored in TLS
struct wxThreadSpecificInfo
{
    // the thread-specific logger or NULL if the thread is using the global one
    // (this is not used for the main thread which always uses the global
    // logger)
    wxLog *logger;

    // true if logging is currently disabled for this thread (this is also not
    // used for the main thread which uses wxLog::ms_doLog)
    //
    // NB: we use a counter-intuitive "disabled" flag instead of "enabled" one
    //     because the default, for 0-initialized struct, should be to enable
    //     logging
    bool loggingDisabled;
};

// currently this is defined in src/common/log.cpp
extern wxTLS_TYPE(wxThreadSpecificInfo) wxThreadInfoVar;
#define wxThreadInfo wxTLS_VALUE(wxThreadInfoVar)

#endif // wxUSE_THREADS

#endif // _WX_PRIVATE_THREADINFO_H_

