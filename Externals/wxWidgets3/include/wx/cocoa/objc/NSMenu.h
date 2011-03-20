///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/objc/NSMenu.h
// Purpose:     WXNSMenu class
// Author:      David Elliott
// Modified by:
// Created:     2007/04/20 (move from NSMenu.mm)
// RCS-ID:      $Id: NSMenu.h 48106 2007-08-15 16:10:19Z DE $
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_OBJC_NSMENU_H__
#define __WX_COCOA_OBJC_NSMENU_H__

#include "wx/cocoa/objc/objc_uniquifying.h"

#import <AppKit/NSMenu.h>

// ============================================================================
// @class WXNSMenu
// ============================================================================
@interface WXNSMenu : NSMenu
{
}

- (void)dealloc;

@end // WXNSMenu
WX_DECLARE_GET_OBJC_CLASS(WXNSMenu,NSMenu)

#endif //ndef __WX_COCOA_OBJC_NSMENU_H__
