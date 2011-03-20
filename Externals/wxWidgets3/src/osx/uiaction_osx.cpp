/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/uiaction_osx.cpp
// Purpose:     wxUIActionSimulator implementation
// Author:      Kevin Ollivier, Steven Lamerton, Vadim Zeitlin
// Modified by:
// Created:     2010-03-06
// RCS-ID:      $Id: uiaction_osx.cpp 66375 2010-12-14 18:43:57Z VZ $
// Copyright:   (c) Kevin Ollivier
//              (c) 2010 Steven Lamerton
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/defs.h"

#if wxUSE_UIACTIONSIMULATOR

#include "wx/uiaction.h"

#include "wx/log.h"

#include "wx/osx/private.h"
#include "wx/osx/core/cfref.h"

namespace
{

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
    
    // adding forward compatible defines for keys that are different on different keyboard layouts
    // see Inside Mac Volume V
    
    enum {
        kVK_ANSI_A = 0x00,
        kVK_ANSI_S = 0x01,
        kVK_ANSI_D = 0x02,
        kVK_ANSI_F = 0x03,
        kVK_ANSI_H = 0x04,
        kVK_ANSI_G = 0x05,
        kVK_ANSI_Z = 0x06,
        kVK_ANSI_X = 0x07,
        kVK_ANSI_C = 0x08,
        kVK_ANSI_V = 0x09,
        kVK_ANSI_B = 0x0B,
        kVK_ANSI_Q = 0x0C,
        kVK_ANSI_W = 0x0D,
        kVK_ANSI_E = 0x0E,
        kVK_ANSI_R = 0x0F,
        kVK_ANSI_Y = 0x10,
        kVK_ANSI_T = 0x11,
        kVK_ANSI_1 = 0x12,
        kVK_ANSI_2 = 0x13,
        kVK_ANSI_3 = 0x14,
        kVK_ANSI_4 = 0x15,
        kVK_ANSI_6 = 0x16,
        kVK_ANSI_5 = 0x17,
        kVK_ANSI_Equal = 0x18,
        kVK_ANSI_9 = 0x19,
        kVK_ANSI_7 = 0x1A,
        kVK_ANSI_Minus = 0x1B,
        kVK_ANSI_8 = 0x1C,
        kVK_ANSI_0 = 0x1D,
        kVK_ANSI_RightBracket = 0x1E,
        kVK_ANSI_O = 0x1F,
        kVK_ANSI_U = 0x20,
        kVK_ANSI_LeftBracket = 0x21,
        kVK_ANSI_I = 0x22,
        kVK_ANSI_P = 0x23,
        kVK_ANSI_L = 0x25,
        kVK_ANSI_J = 0x26,
        kVK_ANSI_Quote = 0x27,
        kVK_ANSI_K = 0x28,
        kVK_ANSI_Semicolon = 0x29,
        kVK_ANSI_Backslash = 0x2A,
        kVK_ANSI_Comma = 0x2B,
        kVK_ANSI_Slash = 0x2C,
        kVK_ANSI_N = 0x2D,
        kVK_ANSI_M = 0x2E,
        kVK_ANSI_Period = 0x2F,
        kVK_ANSI_Grave = 0x32,
        kVK_ANSI_KeypadDecimal = 0x41,
        kVK_ANSI_KeypadMultiply = 0x43,
        kVK_ANSI_KeypadPlus = 0x45,
        kVK_ANSI_KeypadClear = 0x47,
        kVK_ANSI_KeypadDivide = 0x4B,
        kVK_ANSI_KeypadEnter = 0x4C,
        kVK_ANSI_KeypadMinus = 0x4E,
        kVK_ANSI_KeypadEquals = 0x51,
        kVK_ANSI_Keypad0 = 0x52,
        kVK_ANSI_Keypad1 = 0x53,
        kVK_ANSI_Keypad2 = 0x54,
        kVK_ANSI_Keypad3 = 0x55,
        kVK_ANSI_Keypad4 = 0x56,
        kVK_ANSI_Keypad5 = 0x57,
        kVK_ANSI_Keypad6 = 0x58,
        kVK_ANSI_Keypad7 = 0x59,
        kVK_ANSI_Keypad8 = 0x5B,
        kVK_ANSI_Keypad9 = 0x5C
    };
    
    // defines for keys that are the same on all layouts
    
    enum {
        kVK_Return = 0x24,
        kVK_Tab = 0x30,
        kVK_Space = 0x31,
        kVK_Delete = 0x33,
        kVK_Escape = 0x35,
        kVK_Command = 0x37,
        kVK_Shift = 0x38,
        kVK_CapsLock = 0x39,
        kVK_Option = 0x3A,
        kVK_Control = 0x3B,
        kVK_RightShift = 0x3C,
        kVK_RightOption = 0x3D,
        kVK_RightControl = 0x3E,
        kVK_Function = 0x3F,
        kVK_F17 = 0x40,
        kVK_VolumeUp = 0x48,
        kVK_VolumeDown = 0x49,
        kVK_Mute = 0x4A,
        kVK_F18 = 0x4F,
        kVK_F19 = 0x50,
        kVK_F20 = 0x5A,
        kVK_F5 = 0x60,
        kVK_F6 = 0x61,
        kVK_F7 = 0x62,
        kVK_F3 = 0x63,
        kVK_F8 = 0x64,
        kVK_F9 = 0x65,
        kVK_F11 = 0x67,
        kVK_F13 = 0x69,
        kVK_F16 = 0x6A,
        kVK_F14 = 0x6B,
        kVK_F10 = 0x6D,
        kVK_F12 = 0x6F,
        kVK_F15 = 0x71,
        kVK_Help = 0x72,
        kVK_Home = 0x73,
        kVK_PageUp = 0x74,
        kVK_ForwardDelete = 0x75,
        kVK_F4 = 0x76,
        kVK_End = 0x77,
        kVK_F2 = 0x78,
        kVK_PageDown = 0x79,
        kVK_F1 = 0x7A,
        kVK_LeftArrow = 0x7B,
        kVK_RightArrow = 0x7C,
        kVK_DownArrow = 0x7D,
        kVK_UpArrow = 0x7E
    };
    
#endif    
    
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

CGKeyCode wxCharCodeWXToOSX(wxKeyCode code)
{
    CGKeyCode keycode;

    switch (code)
    {
        case 'a': case 'A':   keycode = kVK_ANSI_A; break;
        case 'b': case 'B':   keycode = kVK_ANSI_B; break;
        case 'c': case 'C':   keycode = kVK_ANSI_C; break;
        case 'd': case 'D':   keycode = kVK_ANSI_D; break;
        case 'e': case 'E':   keycode = kVK_ANSI_E; break;
        case 'f': case 'F':   keycode = kVK_ANSI_F; break;
        case 'g': case 'G':   keycode = kVK_ANSI_G; break;
        case 'h': case 'H':   keycode = kVK_ANSI_H; break;
        case 'i': case 'I':   keycode = kVK_ANSI_I; break;
        case 'j': case 'J':   keycode = kVK_ANSI_J; break;
        case 'k': case 'K':   keycode = kVK_ANSI_K; break;
        case 'l': case 'L':   keycode = kVK_ANSI_L; break;
        case 'm': case 'M':   keycode = kVK_ANSI_M; break;
        case 'n': case 'N':   keycode = kVK_ANSI_N; break;
        case 'o': case 'O':   keycode = kVK_ANSI_O; break;
        case 'p': case 'P':   keycode = kVK_ANSI_P; break;
        case 'q': case 'Q':   keycode = kVK_ANSI_Q; break;
        case 'r': case 'R':   keycode = kVK_ANSI_R; break;
        case 's': case 'S':   keycode = kVK_ANSI_S; break;
        case 't': case 'T':   keycode = kVK_ANSI_T; break;
        case 'u': case 'U':   keycode = kVK_ANSI_U; break;
        case 'v': case 'V':   keycode = kVK_ANSI_V; break;
        case 'w': case 'W':   keycode = kVK_ANSI_W; break;
        case 'x': case 'X':   keycode = kVK_ANSI_X; break;
        case 'y': case 'Y':   keycode = kVK_ANSI_Y; break;
        case 'z': case 'Z':   keycode = kVK_ANSI_Z; break;

        case '0':             keycode = kVK_ANSI_0; break;
        case '1':             keycode = kVK_ANSI_1; break;
        case '2':             keycode = kVK_ANSI_2; break;
        case '3':             keycode = kVK_ANSI_3; break;
        case '4':             keycode = kVK_ANSI_4; break;
        case '5':             keycode = kVK_ANSI_5; break;
        case '6':             keycode = kVK_ANSI_6; break;
        case '7':             keycode = kVK_ANSI_7; break;
        case '8':             keycode = kVK_ANSI_8; break;
        case '9':             keycode = kVK_ANSI_9; break;

        case WXK_BACK:        keycode = kVK_Delete; break;
        case WXK_TAB:         keycode = kVK_Tab; break;
        case WXK_RETURN:      keycode = kVK_Return; break;
        case WXK_ESCAPE:      keycode = kVK_Escape; break;
        case WXK_SPACE:       keycode = kVK_Space; break;
        case WXK_DELETE:      keycode = kVK_Delete; break;

        case WXK_SHIFT:       keycode = kVK_Shift; break;
        case WXK_ALT:         keycode = kVK_Option; break;
        case WXK_CONTROL:     keycode = kVK_Control; break;
        case WXK_COMMAND:     keycode = kVK_Command; break;

        case WXK_CAPITAL:     keycode = kVK_CapsLock; break;
        case WXK_END:         keycode = kVK_End; break;
        case WXK_HOME:        keycode = kVK_Home; break;
        case WXK_LEFT:        keycode = kVK_LeftArrow; break;
        case WXK_UP:          keycode = kVK_UpArrow; break;
        case WXK_RIGHT:       keycode = kVK_RightArrow; break;
        case WXK_DOWN:        keycode = kVK_DownArrow; break;

        case WXK_HELP:        keycode = kVK_Help; break;


        case WXK_NUMPAD0:     keycode = kVK_ANSI_Keypad0; break;
        case WXK_NUMPAD1:     keycode = kVK_ANSI_Keypad1; break;
        case WXK_NUMPAD2:     keycode = kVK_ANSI_Keypad2; break;
        case WXK_NUMPAD3:     keycode = kVK_ANSI_Keypad3; break;
        case WXK_NUMPAD4:     keycode = kVK_ANSI_Keypad4; break;
        case WXK_NUMPAD5:     keycode = kVK_ANSI_Keypad5; break;
        case WXK_NUMPAD6:     keycode = kVK_ANSI_Keypad6; break;
        case WXK_NUMPAD7:     keycode = kVK_ANSI_Keypad7; break;
        case WXK_NUMPAD8:     keycode = kVK_ANSI_Keypad8; break;
        case WXK_NUMPAD9:     keycode = kVK_ANSI_Keypad9; break;
        case WXK_F1:          keycode = kVK_F1; break;
        case WXK_F2:          keycode = kVK_F2; break;
        case WXK_F3:          keycode = kVK_F3; break;
        case WXK_F4:          keycode = kVK_F4; break;
        case WXK_F5:          keycode = kVK_F5; break;
        case WXK_F6:          keycode = kVK_F6; break;
        case WXK_F7:          keycode = kVK_F7; break;
        case WXK_F8:          keycode = kVK_F8; break;
        case WXK_F9:          keycode = kVK_F9; break;
        case WXK_F10:         keycode = kVK_F10; break;
        case WXK_F11:         keycode = kVK_F11; break;
        case WXK_F12:         keycode = kVK_F12; break;
        case WXK_F13:         keycode = kVK_F13; break;
        case WXK_F14:         keycode = kVK_F14; break;
        case WXK_F15:         keycode = kVK_F15; break;
        case WXK_F16:         keycode = kVK_F16; break;
        case WXK_F17:         keycode = kVK_F17; break;
        case WXK_F18:         keycode = kVK_F18; break;
        case WXK_F19:         keycode = kVK_F19; break;
        case WXK_F20:         keycode = kVK_F20; break;

        case WXK_PAGEUP:      keycode = kVK_PageUp; break;
        case WXK_PAGEDOWN:    keycode = kVK_PageDown; break;

        case WXK_NUMPAD_DELETE:    keycode = kVK_ANSI_KeypadClear; break;
        case WXK_NUMPAD_EQUAL:     keycode = kVK_ANSI_KeypadEquals; break;
        case WXK_NUMPAD_MULTIPLY:  keycode = kVK_ANSI_KeypadMultiply; break;
        case WXK_NUMPAD_ADD:       keycode = kVK_ANSI_KeypadPlus; break;
        case WXK_NUMPAD_SUBTRACT:  keycode = kVK_ANSI_KeypadMinus; break;
        case WXK_NUMPAD_DECIMAL:   keycode = kVK_ANSI_KeypadDecimal; break;
        case WXK_NUMPAD_DIVIDE:    keycode = kVK_ANSI_KeypadDivide; break;

        default:
            wxLogDebug( "Unrecognised keycode %d", code );
            keycode = static_cast<CGKeyCode>(-1);
    }

    return keycode;
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

