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

#ifndef SFML_TOUCH_HPP
#define SFML_TOUCH_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Export.hpp>
#include <SFML/System/Vector2.hpp>


namespace sf
{
class Window;

////////////////////////////////////////////////////////////
/// \brief Give access to the real-time state of the touches
///
////////////////////////////////////////////////////////////
class SFML_WINDOW_API Touch
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Check if a touch event is currently down
    ///
    /// \param finger Finger index
    ///
    /// \return True if \a finger is currently touching the screen, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isDown(unsigned int finger);

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
    static Vector2i getPosition(unsigned int finger);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current position of a touch in window coordinates
    ///
    /// This function returns the current touch position
    /// relative to the given window.
    ///
    /// \param finger Finger index
    /// \param relativeTo Reference window
    ///
    /// \return Current position of \a finger, or undefined if it's not down
    ///
    ////////////////////////////////////////////////////////////
    static Vector2i getPosition(unsigned int finger, const Window& relativeTo);
};

} // namespace sf


#endif // SFML_TOUCH_HPP


////////////////////////////////////////////////////////////
/// \class sf::Touch
/// \ingroup window
///
/// sf::Touch provides an interface to the state of the
/// touches. It only contains static functions, so it's not
/// meant to be instantiated.
///
/// This class allows users to query the touches state at any
/// time and directly, without having to deal with a window and
/// its events. Compared to the TouchBegan, TouchMoved
/// and TouchEnded events, sf::Touch can retrieve the
/// state of the touches at any time (you don't need to store and
/// update a boolean on your side in order to know if a touch is down),
/// and you always get the real state of the touches, even if they
/// happen when your window is out of focus and no event is triggered.
///
/// The getPosition function can be used to retrieve the current
/// position of a touch. There are two versions: one that operates
/// in global coordinates (relative to the desktop) and one that
/// operates in window coordinates (relative to a specific window).
///
/// Touches are identified by an index (the "finger"), so that in
/// multi-touch events, individual touches can be tracked correctly.
/// As long as a finger touches the screen, it will keep the same index
/// even if other fingers start or stop touching the screen in the
/// meantime. As a consequence, active touch indices may not always be
/// sequential (i.e. touch number 0 may be released while touch number 1
/// is still down).
///
/// Usage example:
/// \code
/// if (sf::Touch::isDown(0))
/// {
///     // touch 0 is down
/// }
///
/// // get global position of touch 1
/// sf::Vector2i globalPos = sf::Touch::getPosition(1);
///
/// // get position of touch 1 relative to a window
/// sf::Vector2i relativePos = sf::Touch::getPosition(1, window);
/// \endcode
///
/// \see sf::Joystick, sf::Keyboard, sf::Mouse
///
////////////////////////////////////////////////////////////
