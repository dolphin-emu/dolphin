/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/uiaction.cpp
// Purpose:     wxUIActionSimulator implementation
// Author:      Kevin Ollivier, Steven Lamerton, Vadim Zeitlin
// Modified by:
// Created:     2010-03-06
// RCS-ID:      $Id: uiaction.cpp 65523 2010-09-11 10:18:53Z VZ $
// Copyright:   (c) Kevin Ollivier
//              (c) 2010 Steven Lamerton
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_UIACTIONSIMULATOR

#include "wx/uiaction.h"
#include "wx/msw/wrapwin.h"

#include "wx/msw/private/keyboard.h"

namespace
{

DWORD EventTypeForMouseButton(int button, bool isDown)
{
    switch (button)
    {
        case wxMOUSE_BTN_LEFT:
            return isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;

        case wxMOUSE_BTN_RIGHT:
            return isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;

        case wxMOUSE_BTN_MIDDLE:
            return isDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;

        default:
            wxFAIL_MSG("Unsupported button passed in.");
            return (DWORD)-1;
    }
}

} // anonymous namespace

bool wxUIActionSimulator::MouseDown(int button)
{
    POINT p;
    GetCursorPos(&p);
    mouse_event(EventTypeForMouseButton(button, true), p.x, p.y, 0, 0);
    return true;
}

bool wxUIActionSimulator::MouseMove(long x, long y)
{
    // Because MOUSEEVENTF_ABSOLUTE takes measurements scaled between 0 & 65535
    // we need to scale our input too
    int displayx, displayy, scaledx, scaledy;
    wxDisplaySize(&displayx, &displayy);
    scaledx = ((float)x / displayx) * 65535;
    scaledy = ((float)y / displayy) * 65535;
    mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE, scaledx, scaledy, 0, 0);
    return true;
}

bool wxUIActionSimulator::MouseUp(int button)
{
    POINT p;
    GetCursorPos(&p);
    mouse_event(EventTypeForMouseButton(button, false), p.x, p.y, 0, 0);
    return true;
}

bool
wxUIActionSimulator::DoKey(int keycode, int WXUNUSED(modifiers), bool isDown)
{
    bool isExtended;
    DWORD vkkeycode = wxMSWKeyboard::WXToVK(keycode, &isExtended);

    DWORD flags = 0;
    if ( isExtended )
        flags |= KEYEVENTF_EXTENDEDKEY;
    if ( !isDown )
        flags |= KEYEVENTF_KEYUP;

    keybd_event(vkkeycode, 0, flags, 0);

    return true;
}

#endif // wxUSE_UIACTIONSIMULATOR
