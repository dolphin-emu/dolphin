///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSPanel.h
// Purpose:     wxCocoaNSPanel class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// RCS-ID:      $Id: NSPanel.h 42046 2006-10-16 09:30:01Z ABX $
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSPANEL_H__
#define __WX_COCOA_NSPANEL_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSPanel);

class wxCocoaNSPanel
{
    WX_DECLARE_OBJC_INTERFACE(NSPanel)
};

#endif // _WX_COCOA_NSPANEL_H_
