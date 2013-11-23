/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/uiactionx11.cpp
// Purpose:     wxUIActionSimulator implementation
// Author:      Kevin Ollivier, Steven Lamerton, Vadim Zeitlin
// Modified by:
// Created:     2010-03-06
// Copyright:   (c) Kevin Ollivier
//              (c) 2010 Steven Lamerton
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/defs.h"

#if wxUSE_UIACTIONSIMULATOR

#include "wx/uiaction.h"
#include "wx/event.h"
#include "wx/evtloop.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "wx/unix/utilsx11.h"

namespace
{

void SendButtonEvent(int button, bool isDown)
{
    int xbutton;
    switch (button)
    {
        case wxMOUSE_BTN_LEFT:
            xbutton = 1;
            break;
        case wxMOUSE_BTN_MIDDLE:
            xbutton = 2;
            break;
        case wxMOUSE_BTN_RIGHT:
            xbutton = 3;
            break;
        default:
            wxFAIL_MSG("Unsupported button passed in.");
            return;
    }

    wxX11Display display;
    wxCHECK_RET(display, "No display available!");

    XEvent event;
    memset(&event, 0x00, sizeof(event));

    event.type = isDown ? ButtonPress : ButtonRelease;
    event.xbutton.button = xbutton;
    event.xbutton.same_screen = True;

    XQueryPointer(display, display.DefaultRoot(),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);
    event.xbutton.subwindow = event.xbutton.window;

    while (event.xbutton.subwindow)
    {
        event.xbutton.window = event.xbutton.subwindow;
        XQueryPointer(display, event.xbutton.window,
                      &event.xbutton.root, &event.xbutton.subwindow,
                      &event.xbutton.x_root, &event.xbutton.y_root,
                      &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);
    }

    XSendEvent(display, PointerWindow, True, 0xfff, &event);
}

} // anonymous namespace

bool wxUIActionSimulator::MouseDown(int button)
{
    SendButtonEvent(button, true);
    return true;
}

bool wxUIActionSimulator::MouseMove(long x, long y)
{
    wxX11Display display;
    wxASSERT_MSG(display, "No display available!");

    Window root = display.DefaultRoot();
    XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);

    // At least with wxGTK we must always process the pending events before the
    // mouse position change really takes effect, so just do it from here
    // instead of forcing the client code using this function to always use
    // wxYield() which is unnecessary under the other platforms.
    if ( wxEventLoopBase* const loop = wxEventLoop::GetActive() )
    {
        loop->YieldFor(wxEVT_CATEGORY_USER_INPUT);
    }

    return true;
}

bool wxUIActionSimulator::MouseUp(int button)
{
    SendButtonEvent(button, false);
    return true;
}

bool wxUIActionSimulator::DoKey(int keycode, int modifiers, bool isDown)
{
    wxX11Display display;
    wxCHECK_MSG(display, false, "No display available!");

    int mask, type;

    if ( isDown )
    {
        type = KeyPress;
        mask = KeyPressMask;
    }
    else
    {
        type = KeyRelease;
        mask = KeyReleaseMask;
    }

    WXKeySym xkeysym = wxCharCodeWXToX(keycode);
    KeyCode xkeycode = XKeysymToKeycode(display, xkeysym);
    if ( xkeycode == NoSymbol )
        return false;

    Window focus;
    int revert;
    XGetInputFocus(display, &focus, &revert);
    if (focus == None)
        return false;

    int mod = 0;

    if (modifiers & wxMOD_SHIFT)
        mod |= ShiftMask;
    //Mod1 is alt in the vast majority of cases
    if (modifiers & wxMOD_ALT)
        mod |= Mod1Mask;
    if (modifiers & wxMOD_CMD)
        mod |= ControlMask;

    XKeyEvent event;
    event.display = display;
    event.window = focus;
    event.root = DefaultRootWindow(event.display);
    event.subwindow = None;
    event.time = CurrentTime;
    event.x = 1;
    event.y = 1;
    event.x_root = 1;
    event.y_root = 1;
    event.same_screen = True;
    event.type = type;
    event.state = mod;
    event.keycode = xkeycode;

    XSendEvent(event.display, event.window, True, mask, (XEvent*) &event);

    return true;
}

#endif // wxUSE_UIACTIONSIMULATOR
