////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
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

#ifndef SFML_INPUTIMPLANDROID_HPP
#define SFML_INPUTIMPLANDROID_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief iOS implementation of inputs (keyboard + mouse)
///
////////////////////////////////////////////////////////////
class InputImpl
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Check if a key is pressed
    ///
    /// \param key Key to check
    ///
    /// \return True if the key is pressed, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isKeyPressed(Keyboard::Key key);

    ////////////////////////////////////////////////////////////
    /// \brief Show or hide the virtual keyboard
    ///
    /// \param visible True to show, false to hide
    ///
    ////////////////////////////////////////////////////////////
    static void setVirtualKeyboardVisible(bool visible);

    ////////////////////////////////////////////////////////////
    /// \brief Check if a mouse button is pressed
    ///
    /// \param button Button to check
    ///
    /// \return True if the button is pressed, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isMouseButtonPressed(Mouse::Button button);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current position of the mouse in desktop coordinates
    ///
    /// This function returns the current position of the mouse
    /// cursor, in global (desktop) coordinates.
    ///
    /// \return Current position of the mouse
    ///
    ////////////////////////////////////////////////////////////
    static Vector2i getMousePosition();

    ////////////////////////////////////////////////////////////
    /// \brief Get the current position of the mouse in window coordinates
    ///
    /// This function returns the current position of the mouse
    /// cursor, relative to the given window.
    /// If no window is used, it returns desktop coordinates.
    ///
    /// \param relativeTo Reference window
    ///
    /// \return Current position of the mouse
    ///
    ////////////////////////////////////////////////////////////
    static Vector2i getMousePosition(const Window& relativeTo);

    ////////////////////////////////////////////////////////////
    /// \brief Set the current position of the mouse in desktop coordinates
    ///
    /// This function sets the current position of the mouse
    /// cursor in global (desktop) coordinates.
    /// If no window is used, it sets the position in desktop coordinates.
    ///
    /// \param position New position of the mouse
    ///
    ////////////////////////////////////////////////////////////
    static void setMousePosition(const Vector2i& position);

    ////////////////////////////////////////////////////////////
    /// \brief Set the current position of the mouse in window coordinates
    ///
    /// This function sets the current position of the mouse
    /// cursor, relative to the given window.
    /// If no window is used, it sets the position in desktop coordinates.
    ///
    /// \param position New position of the mouse
    /// \param relativeTo Reference window
    ///
    ////////////////////////////////////////////////////////////
    static void setMousePosition(const Vector2i& position, const Window& relativeTo);

    ////////////////////////////////////////////////////////////
    /// \brief Check if a touch event is currently down
    ///
    /// \param finger Finger index
    ///
    /// \return True if \a finger is currently touching the screen, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isTouchDown(unsigned int finger);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current position of a touch in desktop coordinates
    ///
    /// This function returns the current touch position
    /// in global (desktop) coordinates.
    ///
    /// \param finger Finger index
    ///
    /// \return Current position of \a finger, or undefined if it's not down
    ///
    ////////////////////////////////////////////////////////////
    static Vector2i getTouchPosition(unsigned int finger);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current position of a touch in window coordinates
    ///
    /// This function returns the current touch position
    /// in global (desktop) coordinates.
    ///
    /// \param finger Finger index
    /// \param relativeTo Reference window
    ///
    /// \return Current position of \a finger, or undefined if it's not down
    ///
    ////////////////////////////////////////////////////////////
    static Vector2i getTouchPosition(unsigned int finger, const Window& relativeTo);
};

} // namespace priv

} // namespace sf


#endif // SFML_INPUTIMPLANDROID_HPP
