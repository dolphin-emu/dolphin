///////////////////////////////////////////////////////////////////////////////
// Name:        wx/nonownedwnd.h
// Purpose:     declares wxNonTopLevelWindow class
// Author:      Vaclav Slavik
// Modified by:
// Created:     2006-12-24
// RCS-ID:      $Id: nonownedwnd.h 54125 2008-06-11 19:17:41Z SC $
// Copyright:   (c) 2006 TT-Solutions
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NONOWNEDWND_H_
#define _WX_NONOWNEDWND_H_

#if defined(__WXDFB__)
    #include "wx/dfb/nonownedwnd.h"
#elif defined(__WXMAC__)
    #include "wx/osx/nonownedwnd.h"
#else
    // other ports can derive both wxTLW and wxPopupWindow directly
    // from wxWindow:
    #include "wx/window.h"
    typedef wxWindow wxNonOwnedWindow;
#endif

#endif // _WX_NONOWNEDWND_H_
