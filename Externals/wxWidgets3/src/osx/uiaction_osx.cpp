/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/uiaction_osx.cpp
// Purpose:     wxUIActionSimulator implementation
// Author:      Kevin Ollivier, Steven Lamerton, Vadim Zeitlin
// Modified by:
// Created:     2010-03-06
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

#include "wx/evtloop.h"

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
            wxFALLTHROUGH;// fall back to the only known remaining case

        case wxMOUSE_BTN_MIDDLE:
            return isDown ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    }
}
    
CGEventType CGEventTypeForMouseDrag(int button)
{
    switch ( button )
    {
        case wxMOUSE_BTN_LEFT:
            return kCGEventLeftMouseDragged;
            break;
            
        case wxMOUSE_BTN_RIGHT:
            return kCGEventRightMouseDragged;
            break;
            
            // All the other buttons use the constant OtherMouseDown but we still
            // want to check for invalid parameters so assert first
        default:
            wxFAIL_MSG("Unsupported button passed in.");
            wxFALLTHROUGH;// fall back to the only known remaining case
            
        case wxMOUSE_BTN_MIDDLE:
            return kCGEventOtherMouseDragged;
            break;
    }

}

CGMouseButton CGButtonForMouseButton(int button)
{
    switch ( button )
    {
        case wxMOUSE_BTN_LEFT:
            return kCGMouseButtonLeft;
            
        case wxMOUSE_BTN_RIGHT:
            return kCGMouseButtonRight;
            
            // All the other buttons use the constant OtherMouseDown but we still
            // want to check for invalid parameters so assert first
        default:
            wxFAIL_MSG("Unsupported button passed in.");
            wxFALLTHROUGH;// fall back to the only known remaining case
            
        case wxMOUSE_BTN_MIDDLE:
            return kCGMouseButtonCenter;
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
            CGEventCreateMouseEvent(NULL, type, GetMousePosition(), CGButtonForMouseButton(button)));

    if ( !event )
        return false;

    CGEventSetType(event, type);
    CGEventPost(tap, event);
    wxCFEventLoop* loop = dynamic_cast<wxCFEventLoop*>(wxEventLoop::GetActive());
    if (loop)
        loop->SetShouldWaitForEvent(true);
    
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

    wxCFEventLoop* loop = dynamic_cast<wxCFEventLoop*>(wxEventLoop::GetActive());
    if (loop)
        loop->SetShouldWaitForEvent(true);
    
    return true;
}

bool wxUIActionSimulator::MouseUp(int button)
{
    CGEventType type = CGEventTypeForMouseButton(button, false);
    wxCFRef<CGEventRef> event(
            CGEventCreateMouseEvent(NULL, type, GetMousePosition(), CGButtonForMouseButton(button)));

    if ( !event )
        return false;

    CGEventSetType(event, type);
    CGEventPost(tap, event);
    wxCFEventLoop* loop = dynamic_cast<wxCFEventLoop*>(wxEventLoop::GetActive());
    if (loop)
        loop->SetShouldWaitForEvent(true);
    
    return true;
}

bool wxUIActionSimulator::MouseDblClick(int button)
{
    CGEventType downtype = CGEventTypeForMouseButton(button, true);
    CGEventType uptype = CGEventTypeForMouseButton(button, false);
    wxCFRef<CGEventRef> event(
                              CGEventCreateMouseEvent(NULL, downtype, GetMousePosition(), CGButtonForMouseButton(button)));
    
    if ( !event )
        return false;
    
    CGEventSetType(event,downtype);
    CGEventPost(tap, event);
    
    CGEventSetType(event, uptype);
    CGEventPost(tap, event);
    
    CGEventSetIntegerValueField(event, kCGMouseEventClickState, 2);
    CGEventSetType(event, downtype);
    CGEventPost(tap, event);
    
    CGEventSetType(event, uptype);
    CGEventPost(tap, event);
    wxCFEventLoop* loop = dynamic_cast<wxCFEventLoop*>(wxEventLoop::GetActive());
    if (loop)
        loop->SetShouldWaitForEvent(true);
    
    return true;
}

bool wxUIActionSimulator::MouseDragDrop(long x1, long y1, long x2, long y2,
                                        int button)
{
    CGPoint pos1,pos2;
    pos1.x = x1;
    pos1.y = y1;
    pos2.x = x2;
    pos2.y = y2;

    CGEventType downtype = CGEventTypeForMouseButton(button, true);
    CGEventType uptype = CGEventTypeForMouseButton(button, false);
    CGEventType dragtype = CGEventTypeForMouseDrag(button) ;

    wxCFRef<CGEventRef> event(
                              CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, pos1, CGButtonForMouseButton(button)));
    
    if ( !event )
        return false;
    
    CGEventSetType(event,kCGEventMouseMoved);
    CGEventPost(tap, event);
    
    CGEventSetType(event,downtype);
    CGEventPost(tap, event);
    
    
    CGEventSetType(event, dragtype);
    CGEventSetLocation(event,pos2);
    CGEventPost(tap, event);
    
    CGEventSetType(event, uptype);
    CGEventPost(tap, event);
    wxCFEventLoop* loop = dynamic_cast<wxCFEventLoop*>(wxEventLoop::GetActive());
    if (loop)
        loop->SetShouldWaitForEvent(true);
    
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
    wxCFEventLoop* loop = dynamic_cast<wxCFEventLoop*>(wxEventLoop::GetActive());
    if (loop)
        loop->SetShouldWaitForEvent(true);

    return true;
}

#endif // wxUSE_UIACTIONSIMULATOR

