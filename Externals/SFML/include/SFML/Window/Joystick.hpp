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

#ifndef SFML_JOYSTICK_HPP
#define SFML_JOYSTICK_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Export.hpp>
#include <SFML/System/String.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Give access to the real-time state of the joysticks
///
////////////////////////////////////////////////////////////
class SFML_WINDOW_API Joystick
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Constants related to joysticks capabilities
    ///
    ////////////////////////////////////////////////////////////
    enum
    {
        Count       = 8,  ///< Maximum number of supported joysticks
        ButtonCount = 32, ///< Maximum number of supported buttons
        AxisCount   = 8   ///< Maximum number of supported axes
    };

    ////////////////////////////////////////////////////////////
    /// \brief Axes supported by SFML joysticks
    ///
    ////////////////////////////////////////////////////////////
    enum Axis
    {
        X,    ///< The X axis
        Y,    ///< The Y axis
        Z,    ///< The Z axis
        R,    ///< The R axis
        U,    ///< The U axis
        V,    ///< The V axis
        PovX, ///< The X axis of the point-of-view hat
        PovY  ///< The Y axis of the point-of-view hat
    };

    ////////////////////////////////////////////////////////////
    /// \brief Structure holding a joystick's identification
    ///
    ////////////////////////////////////////////////////////////
    struct SFML_WINDOW_API Identification
    {
        Identification();

        String       name;      ///< Name of the joystick
        unsigned int vendorId;  ///< Manufacturer identifier
        unsigned int productId; ///< Product identifier
    };

    ////////////////////////////////////////////////////////////
    /// \brief Check if a joystick is connected
    ///
    /// \param joystick Index of the joystick to check
    ///
    /// \return True if the joystick is connected, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isConnected(unsigned int joystick);

    ////////////////////////////////////////////////////////////
    /// \brief Return the number of buttons supported by a joystick
    ///
    /// If the joystick is not connected, this function returns 0.
    ///
    /// \param joystick Index of the joystick
    ///
    /// \return Number of buttons supported by the joystick
    ///
    ////////////////////////////////////////////////////////////
    static unsigned int getButtonCount(unsigned int joystick);

    ////////////////////////////////////////////////////////////
    /// \brief Check if a joystick supports a given axis
    ///
    /// If the joystick is not connected, this function returns false.
    ///
    /// \param joystick Index of the joystick
    /// \param axis     Axis to check
    ///
    /// \return True if the joystick supports the axis, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool hasAxis(unsigned int joystick, Axis axis);

    ////////////////////////////////////////////////////////////
    /// \brief Check if a joystick button is pressed
    ///
    /// If the joystick is not connected, this function returns false.
    ///
    /// \param joystick Index of the joystick
    /// \param button   Button to check
    ///
    /// \return True if the button is pressed, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isButtonPressed(unsigned int joystick, unsigned int button);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current position of a joystick axis
    ///
    /// If the joystick is not connected, this function returns 0.
    ///
    /// \param joystick Index of the joystick
    /// \param axis     Axis to check
    ///
    /// \return Current position of the axis, in range [-100 .. 100]
    ///
    ////////////////////////////////////////////////////////////
    static float getAxisPosition(unsigned int joystick, Axis axis);

    ////////////////////////////////////////////////////////////
    /// \brief Get the joystick information
    ///
    /// \param joystick Index of the joystick
    ///
    /// \return Structure containing joystick information.
    ///
    ////////////////////////////////////////////////////////////
    static Identification getIdentification(unsigned int joystick);

    ////////////////////////////////////////////////////////////
    /// \brief Update the states of all joysticks
    ///
    /// This function is used internally by SFML, so you normally
    /// don't have to call it explicitly. However, you may need to
    /// call it if you have no window yet (or no window at all):
    /// in this case the joystick states are not updated automatically.
    ///
    ////////////////////////////////////////////////////////////
    static void update();
};

} // namespace sf


#endif // SFML_JOYSTICK_HPP


////////////////////////////////////////////////////////////
/// \class sf::Joystick
/// \ingroup window
///
/// sf::Joystick provides an interface to the state of the
/// joysticks. It only contains static functions, so it's not
/// meant to be instantiated. Instead, each joystick is identified
/// by an index that is passed to the functions of this class.
///
/// This class allows users to query the state of joysticks at any
/// time and directly, without having to deal with a window and
/// its events. Compared to the JoystickMoved, JoystickButtonPressed
/// and JoystickButtonReleased events, sf::Joystick can retrieve the
/// state of axes and buttons of joysticks at any time
/// (you don't need to store and update a boolean on your side
/// in order to know if a button is pressed or released), and you
/// always get the real state of joysticks, even if they are
/// moved, pressed or released when your window is out of focus
/// and no event is triggered.
///
/// SFML supports:
/// \li 8 joysticks (sf::Joystick::Count)
/// \li 32 buttons per joystick (sf::Joystick::ButtonCount)
/// \li 8 axes per joystick (sf::Joystick::AxisCount)
///
/// Unlike the keyboard or mouse, the state of joysticks is sometimes
/// not directly available (depending on the OS), therefore an update()
/// function must be called in order to update the current state of
/// joysticks. When you have a window with event handling, this is done
/// automatically, you don't need to call anything. But if you have no
/// window, or if you want to check joysticks state before creating one,
/// you must call sf::Joystick::update explicitly.
///
/// Usage example:
/// \code
/// // Is joystick #0 connected?
/// bool connected = sf::Joystick::isConnected(0);
///
/// // How many buttons does joystick #0 support?
/// unsigned int buttons = sf::Joystick::getButtonCount(0);
///
/// // Does joystick #0 define a X axis?
/// bool hasX = sf::Joystick::hasAxis(0, sf::Joystick::X);
///
/// // Is button #2 pressed on joystick #0?
/// bool pressed = sf::Joystick::isButtonPressed(0, 2);
///
/// // What's the current position of the Y axis on joystick #0?
/// float position = sf::Joystick::getAxisPosition(0, sf::Joystick::Y);
/// \endcode
///
/// \see sf::Keyboard, sf::Mouse
///
////////////////////////////////////////////////////////////
