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
#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/JoystickManager.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
bool Joystick::isConnected(unsigned int joystick)
{
    return priv::JoystickManager::getInstance().getState(joystick).connected;
}


////////////////////////////////////////////////////////////
unsigned int Joystick::getButtonCount(unsigned int joystick)
{
    return priv::JoystickManager::getInstance().getCapabilities(joystick).buttonCount;
}


////////////////////////////////////////////////////////////
bool Joystick::hasAxis(unsigned int joystick, Axis axis)
{
    return priv::JoystickManager::getInstance().getCapabilities(joystick).axes[axis];
}


////////////////////////////////////////////////////////////
bool Joystick::isButtonPressed(unsigned int joystick, unsigned int button)
{
    return priv::JoystickManager::getInstance().getState(joystick).buttons[button];
}


////////////////////////////////////////////////////////////
float Joystick::getAxisPosition(unsigned int joystick, Axis axis)
{
    return priv::JoystickManager::getInstance().getState(joystick).axes[axis];
}


////////////////////////////////////////////////////////////
Joystick::Identification Joystick::getIdentification(unsigned int joystick)
{
    return priv::JoystickManager::getInstance().getIdentification(joystick);
}


////////////////////////////////////////////////////////////
void Joystick::update()
{
    return priv::JoystickManager::getInstance().update();
}


////////////////////////////////////////////////////////////
Joystick::Identification::Identification() :
name     ("No Joystick"),
vendorId (0),
productId(0)
{

}

} // namespace sf
