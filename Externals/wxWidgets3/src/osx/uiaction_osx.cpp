/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/uiaction_osx.cpp
// Purpose:     wxUIActionSimulator implementation
// Author:      Kevin Ollivier, Steven Lamerton, Vadim Zeitlin
// Modified by:
// Created:     2010-03-06
// RCS-ID:      $Id: uiaction_osx.cpp 70402 2012-01-19 15:01:01Z SC $
// Copyright:   (c) Kevin Ollivier
//              (c) 2010 Steven Lamerton
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/object.h"
#endif

#if wxUSE_UIACTIONSIMULATOR

#include "wx/uiaction.h"

#include "wx/log.h"

#include "wx/osx/private.h"
#include "wx/osx/core/cfref.h"

namespace
{
    
CGEventTapLocation tap = kCGSessionEventTap;

CGEventType CGEventTypeForMouseButton(int button, bool isDown)
{
    switch ( button )
    {
        case wxMOUSE_BTN_LEFT:
            return isDown ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;

        case wxMOUSE_BTN_RIGHT:
            return isDown ? kCGEventRightMouseDown : kCGEventRightMouseUp;

        // All the other buttons use the constant OtherMouseDown but we still
        // want to check for invalid parameters so assert first
        default:
            wxFAIL_MSG("Unsupported button passed in.");
            // fall back to the only known remaining case

        case wxMOUSE_BTN_MIDDLE:
            return isDown ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    }
}

CGPoint GetMousePosition()
{
    int x, y;
    wxGetMousePosition(&x, &y);

    CGPoint pos;
    pos.x = x;
    pos.y = y;

    return pos;
}

} // anonymous namespace

bool wxUIActionSimulator::MouseDown(int button)
{
    CGEventType type = CGEventTypeForMouseButton(button, true);
    wxCFRef<CGEventRef> event(
            CGEventCreateMouseEvent(NULL, type, GetMousePosition(), button));

    if ( !event )
        return false;

    CGEventSetType(event, type);
    CGEventPost(tap, event);

    return true;
}

bool wxUIActionSimulator::MouseMove(long x, long y)
{
    CGPoint pos;
    pos.x = x;
    pos.y = y;

    CGEventType type = kCGEventMouseMoved;
    wxCFRef<CGEventRef> event(
            CGEventCreateMouseEvent(NULL, type, pos, kCGMouseButtonLeft));

    if ( !event )
        return false;

    CGEventSetType(event, type);
    CGEventPost(tap, event);

    return true;
}

bool wxUIActionSimulator::MouseUp(int button)
{
    CGEventType type = CGEventTypeForMouseButton(button, false);
    wxCFRef<CGEventRef> event(
            CGEventCreateMouseEvent(NULL, type, GetMousePosition(), button));

    if ( !event )
        return false;

    CGEventSetType(event, type);
    CGEventPost(tap, event);

    return true;
}

bool
wxUIActionSimulator::DoKey(int keycode, int WXUNUSED(modifiers), bool isDown)
{
    CGKeyCode cgcode = wxCharCodeWXToOSX((wxKeyCode)keycode);

    wxCFRef<CGEventRef>
        event(CGEventCreateKeyboardEvent(NULL, cgcode, isDown));
    if ( !event )
        return false;

    CGEventPost(kCGHIDEventTap, event);
    return true;
}

#endif // wxUSE_UIACTIONSIMULATOR

