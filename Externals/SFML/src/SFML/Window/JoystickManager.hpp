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

#ifndef SFML_JOYSTICKMANAGER_HPP
#define SFML_JOYSTICKMANAGER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/JoystickImpl.hpp>
#include <SFML/System/NonCopyable.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Global joystick manager
///
////////////////////////////////////////////////////////////
class JoystickManager : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Get the global unique instance of the manager
    ///
    /// \return Unique instance of the joystick manager
    ///
    ////////////////////////////////////////////////////////////
    static JoystickManager& getInstance();

    ////////////////////////////////////////////////////////////
    /// \brief Get the capabilities for an open joystick
    ///
    /// \param joystick Index of the joystick
    ///
    /// \return Capabilities of the joystick
    ///
    ////////////////////////////////////////////////////////////
    const JoystickCaps& getCapabilities(unsigned int joystick) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the current state of an open joystick
    ///
    /// \param joystick Index of the joystick
    ///
    /// \return Current state of the joystick
    ///
    ////////////////////////////////////////////////////////////
    const JoystickState& getState(unsigned int joystick) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the identification for an open joystick
    ///
    /// \param joystick Index of the joystick
    ///
    /// \return Identification for the joystick
    ///
    ////////////////////////////////////////////////////////////
    const Joystick::Identification& getIdentification(unsigned int joystick) const;

    ////////////////////////////////////////////////////////////
    /// \brief Update the state of all the joysticks
    ///
    ////////////////////////////////////////////////////////////
    void update();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    JoystickManager();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~JoystickManager();

    ////////////////////////////////////////////////////////////
    /// \brief Joystick information and state
    ///
    ////////////////////////////////////////////////////////////
    struct Item
    {
        JoystickImpl             joystick;       ///< Joystick implementation
        JoystickState            state;          ///< The current joystick state
        JoystickCaps             capabilities;   ///< The joystick capabilities
        Joystick::Identification identification; ///< The joystick identification
    };

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Item m_joysticks[Joystick::Count]; ///< Joysticks information and state
};

} // namespace priv

} // namespace sf


#endif // SFML_JOYSTICKMANAGER_HPP
