/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dde.h
// Purpose:     DDE base header
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// RCS-ID:      $Id: dde.h 70808 2012-03-04 20:31:42Z VZ $
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DDE_H_BASE_
#define _WX_DDE_H_BASE_

#include "wx/list.h"

class WXDLLIMPEXP_FWD_BASE wxDDEClient;
class WXDLLIMPEXP_FWD_BASE wxDDEServer;
class WXDLLIMPEXP_FWD_BASE wxDDEConnection;

WX_DECLARE_USER_EXPORTED_LIST(wxDDEClient, wxDDEClientList, WXDLLIMPEXP_BASE);
WX_DECLARE_USER_EXPORTED_LIST(wxDDEServer, wxDDEServerList, WXDLLIMPEXP_BASE);
WX_DECLARE_USER_EXPORTED_LIST(wxDDEConnection, wxDDEConnectionList, WXDLLIMPEXP_BASE);

#if defined(__WINDOWS__)
    #include "wx/msw/dde.h"
#else
    #error DDE is only supported under Windows
#endif

#endif
    // _WX_DDE_H_BASE_
