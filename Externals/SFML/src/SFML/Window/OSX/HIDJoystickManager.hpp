////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Marco Antognini (antognini.marco@gmail.com),
//                         Laurent Gomila (laurent@sfml-dev.org)
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

#ifndef SFML_HIDJOYSTICKMANAGER_HPP
#define SFML_HIDJOYSTICKMANAGER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/NonCopyable.hpp>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDManager.h>

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief sf::priv::InputImpl helper
///
/// This class manage as a singleton instance the joysticks.
/// It's only purpose is to help sf::priv::JoystickImpl class.
///
////////////////////////////////////////////////////////////
class HIDJoystickManager : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Get the unique instance of the class
    ///
    /// \note Private use only
    ///
    /// \return Reference to the HIDJoystickManager instance
    ///
    ////////////////////////////////////////////////////////////
    static HIDJoystickManager& getInstance();

public:

    ////////////////////////////////////////////////////////////
    /// \brief Get the number of currently connected joystick
    ///
    ////////////////////////////////////////////////////////////
    unsigned int getJoystickCount();

    ////////////////////////////////////////////////////////////
    /// \brief Copy the devices associated with this HID manager
    ///
    /// \return a retained CFSetRef of IOHIDDeviceRef or NULL
    ///
    ////////////////////////////////////////////////////////////
    CFSetRef copyJoysticks();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    HIDJoystickManager();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~HIDJoystickManager();

    ////////////////////////////////////////////////////////////
    /// \brief Make sure all event have been processed in the run loop
    ///
    ////////////////////////////////////////////////////////////
    void update();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Private "plug-in" callback
    /// \note Only 'context' parameter is used.
    /// \see IOHIDDeviceCallback
    ///
    ////////////////////////////////////////////////////////////
    static void pluggedIn(void* context, IOReturn, void*, IOHIDDeviceRef);

    ////////////////////////////////////////////////////////////
    /// \brief Private "plug-out" callback
    /// \note Only 'context' parameter is used.
    /// \see IOHIDDeviceCallback
    ///
    ////////////////////////////////////////////////////////////
    static void pluggedOut(void* context, IOReturn, void*, IOHIDDeviceRef);

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    IOHIDManagerRef m_manager;      ///< HID Manager
    unsigned int    m_joystickCount;///< Number of joysticks currently connected
};


} // namespace priv

} // namespace sf

#endif
