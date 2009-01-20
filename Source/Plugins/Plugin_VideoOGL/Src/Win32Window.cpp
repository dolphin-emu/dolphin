///////////////////////////////////////////////////////////
/// Convert a Win32 virtual key code to a SFML key code
////////////////////////////////////////////////////////////
Key::Code VirtualKeyCodeToSF(WPARAM VirtualKey, LPARAM Flags)
{
    switch (VirtualKey)
    {
        // VK_SHIFT is handled by the GetShiftState function
        case VK_MENU :       return (Flags & (1 << 24)) ? Key::RAlt     : Key::LAlt;
        case VK_CONTROL :    return (Flags & (1 << 24)) ? Key::RControl : Key::LControl;
        case VK_LWIN :       return Key::LSystem;
        case VK_RWIN :       return Key::RSystem;
        case VK_APPS :       return Key::Menu;
        case VK_OEM_1 :      return Key::SemiColon;
        case VK_OEM_2 :      return Key::Slash;
        case VK_OEM_PLUS :   return Key::Equal;
        case VK_OEM_MINUS :  return Key::Dash;
        case VK_OEM_4 :      return Key::LBracket;
        case VK_OEM_6 :      return Key::RBracket;
        case VK_OEM_COMMA :  return Key::Comma;
        case VK_OEM_PERIOD : return Key::Period;
        case VK_OEM_7 :      return Key::Quote;
        case VK_OEM_5 :      return Key::BackSlash;
        case VK_OEM_3 :      return Key::Tilde;
        case VK_ESCAPE :     return Key::Escape;
        case VK_SPACE :      return Key::Space;
        case VK_RETURN :     return Key::Return;
        case VK_BACK :       return Key::Back;
        case VK_TAB :        return Key::Tab;
        case VK_PRIOR :      return Key::PageUp;
        case VK_NEXT :       return Key::PageDown;
        case VK_END :        return Key::End;
        case VK_HOME :       return Key::Home;
        case VK_INSERT :     return Key::Insert;
        case VK_DELETE :     return Key::Delete;
        case VK_ADD :        return Key::Add;
        case VK_SUBTRACT :   return Key::Subtract;
        case VK_MULTIPLY :   return Key::Multiply;
        case VK_DIVIDE :     return Key::Divide;
        case VK_PAUSE :      return Key::Pause;
        case VK_F1 :         return Key::F1;
        case VK_F2 :         return Key::F2;
        case VK_F3 :         return Key::F3;
        case VK_F4 :         return Key::F4;
        case VK_F5 :         return Key::F5;
        case VK_F6 :         return Key::F6;
        case VK_F7 :         return Key::F7;
        case VK_F8 :         return Key::F8;
        case VK_F9 :         return Key::F9;
        case VK_F10 :        return Key::F10;
        case VK_F11 :        return Key::F11;
        case VK_F12 :        return Key::F12;
        case VK_F13 :        return Key::F13;
        case VK_F14 :        return Key::F14;
        case VK_F15 :        return Key::F15;
        case VK_RIGHT :      return Key::Right;
        case VK_UP :         return Key::Up;
        case VK_DOWN :       return Key::Down;
        case VK_NUMPAD0 :    return Key::Numpad0;
        case VK_NUMPAD1 :    return Key::Numpad1;
        case VK_NUMPAD2 :    return Key::Numpad2;
        case VK_NUMPAD3 :    return Key::Numpad3;
        case VK_NUMPAD4 :    return Key::Numpad4;
        case VK_NUMPAD5 :    return Key::Numpad5;
        case VK_NUMPAD6 :    return Key::Numpad6;
        case VK_NUMPAD7 :    return Key::Numpad7;
        case VK_NUMPAD8 :    return Key::Numpad8;
        case VK_NUMPAD9 :    return Key::Numpad9;
        case 'A' :           return Key::A;
        case 'Z' :           return Key::Z;
        case 'E' :           return Key::E;
        case 'R' :           return Key::R;
        case 'T' :           return Key::T;
        case 'Y' :           return Key::Y;
        case 'U' :           return Key::U;
        case 'I' :           return Key::I;
        case 'O' :           return Key::O;
        case 'P' :           return Key::P;
        case 'Q' :           return Key::Q;
        case 'S' :           return Key::S;
        case 'D' :           return Key::D;
        case 'F' :           return Key::F;
        case 'G' :           return Key::G;
        case 'H' :           return Key::H;
        case 'J' :           return Key::J;
        case 'K' :           return Key::K;
        case 'L' :           return Key::L;
        case 'M' :           return Key::M;
        case 'W' :           return Key::W;
        case 'X' :           return Key::X;
        case 'C' :           return Key::C;
        case 'V' :           return Key::V;
        case 'B' :           return Key::B;
        case 'N' :           return Key::N;
        case '0' :           return Key::Num0;
        case '1' :           return Key::Num1;
        case '2' :           return Key::Num2;
        case '3' :           return Key::Num3;
        case '4' :           return Key::Num4;
        case '5' :           return Key::Num5;
        case '6' :           return Key::Num6;
        case '7' :           return Key::Num7;
        case '8' :           return Key::Num8;
        case '9' :           return Key::Num9;
    }

    return Key::Code(0);
}

////////////////////////////////////////////////////////////
/// Process a Win32 event
////////////////////////////////////////////////////////////
void ProcessEvent(UINT Message, WPARAM WParam, LPARAM LParam)
{
    // Don't process any message until window is created
    if (myHandle == NULL)
        return;

    switch (Message)
    {
        // Destroy event
        case WM_DESTROY :
        {
            // Here we must cleanup resources !
            Cleanup();
            break;
        }

        // Set cursor event
        case WM_SETCURSOR :
        {
            // The mouse has moved, if the cursor is in our window we must refresh the cursor
            if (LOWORD(LParam) == HTCLIENT)
                SetCursor(myCursor);

            break;
        }
       case WM_CLOSE :
        {
            Event Evt;
            Evt.Type = Event::Closed;
            SendEvent(Evt);
            break;
        }

        // Resize event
        case WM_SIZE :
        {
            // Update window size
            RECT Rect;
            GetClientRect(myHandle, &Rect);
            myWidth  = Rect.right - Rect.left;
            myHeight = Rect.bottom - Rect.top;

            Event Evt;
            Evt.Type        = Event::Resized;
            Evt.Size.Width  = myWidth;
            Evt.Size.Height = myHeight;
            SendEvent(Evt);
            break;
        }
      // Gain focus event
        case WM_SETFOCUS :
        {
            Event Evt;
            Evt.Type = Event::GainedFocus;
            SendEvent(Evt);
            break;
        }

        // Lost focus event
        case WM_KILLFOCUS :
        {
            Event Evt;
            Evt.Type = Event::LostFocus;
            SendEvent(Evt);
            break;
        }

        // Text event
        case WM_CHAR :
        {
            Event Evt;
            Evt.Type = Event::TextEntered;
            Evt.Text.Unicode = static_cast<Uint32>(WParam);
            SendEvent(Evt);
            break;
        }
        // Keydown event
        case WM_KEYDOWN :
        case WM_SYSKEYDOWN :
        {
            if (myKeyRepeatEnabled || ((LParam & (1 << 30)) == 0))
            {
                Event Evt;
                Evt.Type        = Event::KeyPressed;
                Evt.Key.Code    = (WParam == VK_SHIFT) ? GetShiftState(true) : VirtualKeyCodeToSF(WParam, LParam);
                Evt.Key.Alt     = HIWORD(GetAsyncKeyState(VK_MENU))    != 0;
                Evt.Key.Control = HIWORD(GetAsyncKeyState(VK_CONTROL)) != 0;
                Evt.Key.Shift   = HIWORD(GetAsyncKeyState(VK_SHIFT))   != 0;
                SendEvent(Evt);
            }
            break;
        }

        // Keyup event
        case WM_KEYUP :
        case WM_SYSKEYUP :
        {
            Event Evt;
            Evt.Type        = Event::KeyReleased;
            Evt.Key.Code    = (WParam == VK_SHIFT) ? GetShiftState(false) : VirtualKeyCodeToSF(WParam, LParam);
            Evt.Key.Alt     = HIWORD(GetAsyncKeyState(VK_MENU))    != 0;
            Evt.Key.Control = HIWORD(GetAsyncKeyState(VK_CONTROL)) != 0;
            Evt.Key.Shift   = HIWORD(GetAsyncKeyState(VK_SHIFT))   != 0;
         SendEvent(Evt);
            break;
        }

        // Mouse wheel event
        case WM_MOUSEWHEEL :
        {
            Event Evt;
            Evt.Type = Event::MouseWheelMoved;
            Evt.MouseWheel.Delta = static_cast<Int16>(HIWORD(WParam)) / 120;
            SendEvent(Evt);
            break;
        }

        // Mouse left button down event
        case WM_LBUTTONDOWN :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonPressed;
            Evt.MouseButton.Button = Mouse::Left;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse left button up event
        case WM_LBUTTONUP :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonReleased;
            Evt.MouseButton.Button = Mouse::Left;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse right button down event
        case WM_RBUTTONDOWN :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonPressed;
            Evt.MouseButton.Button = Mouse::Right;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse right button up event
        case WM_RBUTTONUP :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonReleased;
            Evt.MouseButton.Button = Mouse::Right;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse wheel button down event
        case WM_MBUTTONDOWN :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonPressed;
            Evt.MouseButton.Button = Mouse::Middle;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse wheel button up event
        case WM_MBUTTONUP :
        {
           Event Evt;
            Evt.Type               = Event::MouseButtonReleased;
            Evt.MouseButton.Button = Mouse::Middle;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse X button down event
        case WM_XBUTTONDOWN :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonPressed;
            Evt.MouseButton.Button = HIWORD(WParam) == XBUTTON1 ? Mouse::XButton1 : Mouse::XButton2;
            Evt.MouseButton.X      = LOWORD(LParam);
            Evt.MouseButton.Y      = HIWORD(LParam);
            SendEvent(Evt);
            break;
        }

        // Mouse X button up event
        case WM_XBUTTONUP :
        {
            Event Evt;
            Evt.Type               = Event::MouseButtonReleased;
            Evt.MouseButton.Button = HIWORD(WParam) == XBUTTON1 ? Mouse::XButton1 : Mouse::XButton2;
            Evt.MouseButton.X      = LOWORD(LParam);
          SendEvent(Evt);
            break;
        }

        // Mouse move event
        case WM_MOUSEMOVE :
        {
            // Check if we need to generate a MouseEntered event
            if (!myIsCursorIn)
            {
                TRACKMOUSEEVENT MouseEvent;
                MouseEvent.cbSize    = sizeof(TRACKMOUSEEVENT);
                MouseEvent.hwndTrack = myHandle;
                MouseEvent.dwFlags   = TME_LEAVE;
                TrackMouseEvent(&MouseEvent);

                myIsCursorIn = true;

                Event Evt;
                Evt.Type = Event::MouseEntered;
                SendEvent(Evt);
            }

            Event Evt;
            Evt.Type        = Event::MouseMoved;
            Evt.MouseMove.X = LOWORD(LParam);
           SendEvent(Evt);
            break;
        }

        // Mouse leave event
        case WM_MOUSELEAVE :
        {
            myIsCursorIn = false;

            Event Evt;
            Evt.Type = Event::MouseLeft;
            SendEvent(Evt);
            break;
        }
    }
}


///////////////////////////////////////////////////////////
/// Check the state of the shift keys on a key event,
/// and return the corresponding SF key code
////////////////////////////////////////////////////////////
Key::Code WindowImplWin32::GetShiftState(bool KeyDown)
{
    static bool LShiftPrevDown = false;
    static bool RShiftPrevDown = false;

    bool LShiftDown = (HIWORD(GetAsyncKeyState(VK_LSHIFT)) != 0);
    bool RShiftDown = (HIWORD(GetAsyncKeyState(VK_RSHIFT)) != 0);

    Key::Code Code = Key::Code(0);
    if (KeyDown)
    {
        if      (!LShiftPrevDown && LShiftDown) Code = Key::LShift;
        else if (!RShiftPrevDown && RShiftDown) Code = Key::RShift;
    }
    else
    {
        if      (LShiftPrevDown && !LShiftDown) Code = Key::LShift;
        else if (RShiftPrevDown && !RShiftDown) Code = Key::RShift;
    }

    LShiftPrevDown = LShiftDown;
    RShiftPrevDown = RShiftDown;

    return Code;
}
