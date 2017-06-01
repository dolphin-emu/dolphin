////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Window.hpp> // important to be included first (conflict with None)
#include <SFML/Window/Unix/InputImpl.hpp>
#include <SFML/Window/Unix/Display.hpp>
#include <SFML/Window/Unix/ScopedXcbPtr.hpp>
#include <SFML/System/Err.hpp>
#include <xcb/xcb.h>
#include <X11/keysym.h>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
bool InputImpl::isKeyPressed(Keyboard::Key key)
{
    // Get the corresponding X11 keysym
    KeySym keysym = 0;
    switch (key)
    {
        case Keyboard::LShift:     keysym = XK_Shift_L;      break;
        case Keyboard::RShift:     keysym = XK_Shift_R;      break;
        case Keyboard::LControl:   keysym = XK_Control_L;    break;
        case Keyboard::RControl:   keysym = XK_Control_R;    break;
        case Keyboard::LAlt:       keysym = XK_Alt_L;        break;
        case Keyboard::RAlt:       keysym = XK_Alt_R;        break;
        case Keyboard::LSystem:    keysym = XK_Super_L;      break;
        case Keyboard::RSystem:    keysym = XK_Super_R;      break;
        case Keyboard::Menu:       keysym = XK_Menu;         break;
        case Keyboard::Escape:     keysym = XK_Escape;       break;
        case Keyboard::SemiColon:  keysym = XK_semicolon;    break;
        case Keyboard::Slash:      keysym = XK_slash;        break;
        case Keyboard::Equal:      keysym = XK_equal;        break;
        case Keyboard::Dash:       keysym = XK_minus;        break;
        case Keyboard::LBracket:   keysym = XK_bracketleft;  break;
        case Keyboard::RBracket:   keysym = XK_bracketright; break;
        case Keyboard::Comma:      keysym = XK_comma;        break;
        case Keyboard::Period:     keysym = XK_period;       break;
        case Keyboard::Quote:      keysym = XK_apostrophe;   break;
        case Keyboard::BackSlash:  keysym = XK_backslash;    break;
        case Keyboard::Tilde:      keysym = XK_grave;        break;
        case Keyboard::Space:      keysym = XK_space;        break;
        case Keyboard::Return:     keysym = XK_Return;       break;
        case Keyboard::BackSpace:  keysym = XK_BackSpace;    break;
        case Keyboard::Tab:        keysym = XK_Tab;          break;
        case Keyboard::PageUp:     keysym = XK_Prior;        break;
        case Keyboard::PageDown:   keysym = XK_Next;         break;
        case Keyboard::End:        keysym = XK_End;          break;
        case Keyboard::Home:       keysym = XK_Home;         break;
        case Keyboard::Insert:     keysym = XK_Insert;       break;
        case Keyboard::Delete:     keysym = XK_Delete;       break;
        case Keyboard::Add:        keysym = XK_KP_Add;       break;
        case Keyboard::Subtract:   keysym = XK_KP_Subtract;  break;
        case Keyboard::Multiply:   keysym = XK_KP_Multiply;  break;
        case Keyboard::Divide:     keysym = XK_KP_Divide;    break;
        case Keyboard::Pause:      keysym = XK_Pause;        break;
        case Keyboard::F1:         keysym = XK_F1;           break;
        case Keyboard::F2:         keysym = XK_F2;           break;
        case Keyboard::F3:         keysym = XK_F3;           break;
        case Keyboard::F4:         keysym = XK_F4;           break;
        case Keyboard::F5:         keysym = XK_F5;           break;
        case Keyboard::F6:         keysym = XK_F6;           break;
        case Keyboard::F7:         keysym = XK_F7;           break;
        case Keyboard::F8:         keysym = XK_F8;           break;
        case Keyboard::F9:         keysym = XK_F9;           break;
        case Keyboard::F10:        keysym = XK_F10;          break;
        case Keyboard::F11:        keysym = XK_F11;          break;
        case Keyboard::F12:        keysym = XK_F12;          break;
        case Keyboard::F13:        keysym = XK_F13;          break;
        case Keyboard::F14:        keysym = XK_F14;          break;
        case Keyboard::F15:        keysym = XK_F15;          break;
        case Keyboard::Left:       keysym = XK_Left;         break;
        case Keyboard::Right:      keysym = XK_Right;        break;
        case Keyboard::Up:         keysym = XK_Up;           break;
        case Keyboard::Down:       keysym = XK_Down;         break;
        case Keyboard::Numpad0:    keysym = XK_KP_Insert;    break;
        case Keyboard::Numpad1:    keysym = XK_KP_End;       break;
        case Keyboard::Numpad2:    keysym = XK_KP_Down;      break;
        case Keyboard::Numpad3:    keysym = XK_KP_Page_Down; break;
        case Keyboard::Numpad4:    keysym = XK_KP_Left;      break;
        case Keyboard::Numpad5:    keysym = XK_KP_Begin;     break;
        case Keyboard::Numpad6:    keysym = XK_KP_Right;     break;
        case Keyboard::Numpad7:    keysym = XK_KP_Home;      break;
        case Keyboard::Numpad8:    keysym = XK_KP_Up;        break;
        case Keyboard::Numpad9:    keysym = XK_KP_Page_Up;   break;
        case Keyboard::A:          keysym = XK_a;            break;
        case Keyboard::B:          keysym = XK_b;            break;
        case Keyboard::C:          keysym = XK_c;            break;
        case Keyboard::D:          keysym = XK_d;            break;
        case Keyboard::E:          keysym = XK_e;            break;
        case Keyboard::F:          keysym = XK_f;            break;
        case Keyboard::G:          keysym = XK_g;            break;
        case Keyboard::H:          keysym = XK_h;            break;
        case Keyboard::I:          keysym = XK_i;            break;
        case Keyboard::J:          keysym = XK_j;            break;
        case Keyboard::K:          keysym = XK_k;            break;
        case Keyboard::L:          keysym = XK_l;            break;
        case Keyboard::M:          keysym = XK_m;            break;
        case Keyboard::N:          keysym = XK_n;            break;
        case Keyboard::O:          keysym = XK_o;            break;
        case Keyboard::P:          keysym = XK_p;            break;
        case Keyboard::Q:          keysym = XK_q;            break;
        case Keyboard::R:          keysym = XK_r;            break;
        case Keyboard::S:          keysym = XK_s;            break;
        case Keyboard::T:          keysym = XK_t;            break;
        case Keyboard::U:          keysym = XK_u;            break;
        case Keyboard::V:          keysym = XK_v;            break;
        case Keyboard::W:          keysym = XK_w;            break;
        case Keyboard::X:          keysym = XK_x;            break;
        case Keyboard::Y:          keysym = XK_y;            break;
        case Keyboard::Z:          keysym = XK_z;            break;
        case Keyboard::Num0:       keysym = XK_0;            break;
        case Keyboard::Num1:       keysym = XK_1;            break;
        case Keyboard::Num2:       keysym = XK_2;            break;
        case Keyboard::Num3:       keysym = XK_3;            break;
        case Keyboard::Num4:       keysym = XK_4;            break;
        case Keyboard::Num5:       keysym = XK_5;            break;
        case Keyboard::Num6:       keysym = XK_6;            break;
        case Keyboard::Num7:       keysym = XK_7;            break;
        case Keyboard::Num8:       keysym = XK_8;            break;
        case Keyboard::Num9:       keysym = XK_9;            break;
        default:                   keysym = 0;               break;
    }

    // Sanity checks
    if (key < 0 || key >= sf::Keyboard::KeyCount)
        return false;

    // Open a connection with the X server
    Display* display = OpenDisplay();

    // Convert to keycode
    xcb_keycode_t keycode = XKeysymToKeycode(display, keysym);

    CloseDisplay(display);

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    // Open a connection with the X server
    xcb_connection_t* connection = OpenConnection();

    // Get the whole keyboard state
    ScopedXcbPtr<xcb_query_keymap_reply_t> keymap(
        xcb_query_keymap_reply(
            connection,
            xcb_query_keymap(connection),
            &error
        )
    );

    // Close the connection with the X server
    CloseConnection(connection);

    if (error)
    {
        err() << "Failed to query keymap" << std::endl;

        return false;
    }

    // Check our keycode
    return (keymap->keys[keycode / 8] & (1 << (keycode % 8))) != 0;
}


////////////////////////////////////////////////////////////
void InputImpl::setVirtualKeyboardVisible(bool /*visible*/)
{
    // Not applicable
}


////////////////////////////////////////////////////////////
bool InputImpl::isMouseButtonPressed(Mouse::Button button)
{
    // Open a connection with the X server
    xcb_connection_t* connection = OpenConnection();

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    // Get pointer mask
    ScopedXcbPtr<xcb_query_pointer_reply_t> pointer(
        xcb_query_pointer_reply(
            connection,
            xcb_query_pointer(
                connection,
                XCBDefaultRootWindow(connection)
            ),
            &error
        )
    );

    // Close the connection with the X server
    CloseConnection(connection);

    if (error)
    {
        err() << "Failed to query pointer" << std::endl;

        return false;
    }

    uint16_t buttons = pointer->mask;

    switch (button)
    {
        case Mouse::Left:     return buttons & XCB_BUTTON_MASK_1;
        case Mouse::Right:    return buttons & XCB_BUTTON_MASK_3;
        case Mouse::Middle:   return buttons & XCB_BUTTON_MASK_2;
        case Mouse::XButton1: return false; // not supported by X
        case Mouse::XButton2: return false; // not supported by X
        default:              return false;
    }
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition()
{
    // Open a connection with the X server
    xcb_connection_t* connection = OpenConnection();

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    ScopedXcbPtr<xcb_query_pointer_reply_t> pointer(
        xcb_query_pointer_reply(
            connection,
            xcb_query_pointer(
                connection,
                XCBDefaultRootWindow(connection)
            ),
            &error
        )
    );

    // Close the connection with the X server
    CloseConnection(connection);

    if (error)
    {
        err() << "Failed to query pointer" << std::endl;

        return Vector2i(0, 0);
    }

    return Vector2i(pointer->root_x, pointer->root_y);
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition(const Window& relativeTo)
{
    WindowHandle handle = relativeTo.getSystemHandle();
    if (handle)
    {
        // Open a connection with the X server
        xcb_connection_t* connection = OpenConnection();

        ScopedXcbPtr<xcb_generic_error_t> error(NULL);

        ScopedXcbPtr<xcb_query_pointer_reply_t> pointer(
            xcb_query_pointer_reply(
                connection,
                xcb_query_pointer(
                    connection,
                    handle
                ),
                &error
            )
        );

        // Close the connection with the X server
        CloseConnection(connection);

        if (error)
        {
            err() << "Failed to query pointer" << std::endl;

            return Vector2i(0, 0);
        }

        return Vector2i(pointer->win_x, pointer->win_y);
    }
    else
    {
        return Vector2i();
    }
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position)
{
    // Open a connection with the X server
    xcb_connection_t* connection = OpenConnection();

    ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
        connection,
        xcb_warp_pointer(
            connection,
            None,                             // Source window
            XCBDefaultRootWindow(connection), // Destination window
            0, 0,                             // Source position
            0, 0,                             // Source size
            position.x, position.y            // Destination position
        )
    ));

    if (error)
        err() << "Failed to set mouse position" << std::endl;

    xcb_flush(connection);

    // Close the connection with the X server
    CloseConnection(connection);
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position, const Window& relativeTo)
{
    // Open a connection with the X server
    xcb_connection_t* connection = OpenConnection();

    WindowHandle handle = relativeTo.getSystemHandle();
    if (handle)
    {
        ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
            connection,
            xcb_warp_pointer(
                connection,
                None,                  // Source window
                handle,                // Destination window
                0, 0,                  // Source position
                0, 0,                  // Source size
                position.x, position.y // Destination position
            )
        ));

        if (error)
            err() << "Failed to set mouse position" << std::endl;

        xcb_flush(connection);
    }

    // Close the connection with the X server
    CloseConnection(connection);
}


////////////////////////////////////////////////////////////
bool InputImpl::isTouchDown(unsigned int /*finger*/)
{
    // Not applicable
    return false;
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int /*finger*/)
{
    // Not applicable
    return Vector2i();
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int /*finger*/, const Window& /*relativeTo*/)
{
    // Not applicable
    return Vector2i();
}

} // namespace priv

} // namespace sf
