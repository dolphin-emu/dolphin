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
#include <SFML/Window/JoystickImpl.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Err.hpp>
#include <windows.h>
#include <tchar.h>
#include <regstr.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace
{
    struct ConnectionCache
    {
        ConnectionCache() : connected(false) {}
        bool connected;
        sf::Clock timer;
    };

    const sf::Time connectionRefreshDelay = sf::milliseconds(500);
    ConnectionCache connectionCache[sf::Joystick::Count];

    // Get a system error string from an error code
    std::string getErrorString(DWORD error)
    {
        PTCHAR buffer;

        if (FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, reinterpret_cast<PTCHAR>(&buffer), 0, NULL) == 0)
            return "Unknown error.";

        sf::String message = buffer;
        LocalFree(buffer);
        return message.toAnsiString();
    }

    // Get the joystick's name
    sf::String getDeviceName(unsigned int index, JOYCAPS caps)
    {
        // Give the joystick a default name
        sf::String joystickDescription = "Unknown Joystick";

        LONG result;
        HKEY rootKey;
        HKEY currentKey;
        std::basic_string<TCHAR> subkey;

        subkey  = REGSTR_PATH_JOYCONFIG;
        subkey += TEXT('\\');
        subkey += caps.szRegKey;
        subkey += TEXT('\\');
        subkey += REGSTR_KEY_JOYCURR;

        rootKey = HKEY_CURRENT_USER;
        result  = RegOpenKeyEx(rootKey, subkey.c_str(), 0, KEY_READ, &currentKey);

        if (result != ERROR_SUCCESS)
        {
            rootKey = HKEY_LOCAL_MACHINE;
            result  = RegOpenKeyEx(rootKey, subkey.c_str(), 0, KEY_READ, &currentKey);

            if (result != ERROR_SUCCESS)
            {
                sf::err() << "Unable to open registry for joystick at index " << index << ": " << getErrorString(result) << std::endl;
                return joystickDescription;
            }
        }

        std::basic_ostringstream<TCHAR> indexString;
        indexString << index + 1;

        subkey  = TEXT("Joystick");
        subkey += indexString.str();
        subkey += REGSTR_VAL_JOYOEMNAME;

        TCHAR keyData[256];
        DWORD keyDataSize = sizeof(keyData);

        result = RegQueryValueEx(currentKey, subkey.c_str(), NULL, NULL, reinterpret_cast<LPBYTE>(keyData), &keyDataSize);
        RegCloseKey(currentKey);

        if (result != ERROR_SUCCESS)
        {
            sf::err() << "Unable to query registry key for joystick at index " << index << ": " << getErrorString(result) << std::endl;
            return joystickDescription;
        }

        subkey  = REGSTR_PATH_JOYOEM;
        subkey += TEXT('\\');
        subkey.append(keyData, keyDataSize / sizeof(TCHAR));

        result = RegOpenKeyEx(rootKey, subkey.c_str(), 0, KEY_READ, &currentKey);

        if (result != ERROR_SUCCESS)
        {
            sf::err() << "Unable to open registry key for joystick at index " << index << ": " << getErrorString(result) << std::endl;
            return joystickDescription;
        }

        keyDataSize = sizeof(keyData);

        result = RegQueryValueEx(currentKey, REGSTR_VAL_JOYOEMNAME, NULL, NULL, reinterpret_cast<LPBYTE>(keyData), &keyDataSize);
        RegCloseKey(currentKey);

        if (result != ERROR_SUCCESS)
        {
            sf::err() << "Unable to query name for joystick at index " << index << ": " << getErrorString(result) << std::endl;
            return joystickDescription;
        }

        keyData[255] = TEXT('\0'); // Ensure null terminator in case the data is too long.
        joystickDescription = keyData;

        return joystickDescription;
    }
}

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
void JoystickImpl::initialize()
{
    // Perform the initial scan and populate the connection cache
    for (unsigned int i = 0; i < Joystick::Count; ++i)
    {
        ConnectionCache& cache = connectionCache[i];

        // Check if the joystick is connected
        JOYINFOEX joyInfo;
        joyInfo.dwSize = sizeof(joyInfo);
        joyInfo.dwFlags = 0;
        cache.connected = joyGetPosEx(JOYSTICKID1 + i, &joyInfo) == JOYERR_NOERROR;

        // start the timeout
        cache.timer.restart();
    }
}


////////////////////////////////////////////////////////////
void JoystickImpl::cleanup()
{
    // Nothing to do
}


////////////////////////////////////////////////////////////
bool JoystickImpl::isConnected(unsigned int index)
{
    // We check the connection state of joysticks only every N milliseconds,
    // because of a strange (buggy?) behavior of joyGetPosEx when joysticks
    // are just plugged/unplugged -- it takes really long and kills the app performances
    ConnectionCache& cache = connectionCache[index];
    if (cache.timer.getElapsedTime() > connectionRefreshDelay)
    {
        cache.timer.restart();

        JOYINFOEX joyInfo;
        joyInfo.dwSize = sizeof(joyInfo);
        joyInfo.dwFlags = 0;

        cache.connected = joyGetPosEx(JOYSTICKID1 + index, &joyInfo) == JOYERR_NOERROR;
        return cache.connected;
    }
    else
    {
        return cache.connected;
    }
}


////////////////////////////////////////////////////////////
bool JoystickImpl::open(unsigned int index)
{
    // No explicit "open" action is required
    m_index = JOYSTICKID1 + index;

    // Store the joystick capabilities
    bool success = joyGetDevCaps(m_index, &m_caps, sizeof(m_caps)) == JOYERR_NOERROR;

    if (success)
    {
        m_identification.name      = getDeviceName(m_index, m_caps);
        m_identification.productId = m_caps.wPid;
        m_identification.vendorId  = m_caps.wMid;
    }

    return success;
}


////////////////////////////////////////////////////////////
void JoystickImpl::close()
{
    // Nothing to do
}

////////////////////////////////////////////////////////////
JoystickCaps JoystickImpl::getCapabilities() const
{
    JoystickCaps caps;

    caps.buttonCount = m_caps.wNumButtons;
    if (caps.buttonCount > Joystick::ButtonCount)
        caps.buttonCount = Joystick::ButtonCount;

    caps.axes[Joystick::X]    = true;
    caps.axes[Joystick::Y]    = true;
    caps.axes[Joystick::Z]    = (m_caps.wCaps & JOYCAPS_HASZ) != 0;
    caps.axes[Joystick::R]    = (m_caps.wCaps & JOYCAPS_HASR) != 0;
    caps.axes[Joystick::U]    = (m_caps.wCaps & JOYCAPS_HASU) != 0;
    caps.axes[Joystick::V]    = (m_caps.wCaps & JOYCAPS_HASV) != 0;
    caps.axes[Joystick::PovX] = (m_caps.wCaps & JOYCAPS_HASPOV) != 0;
    caps.axes[Joystick::PovY] = (m_caps.wCaps & JOYCAPS_HASPOV) != 0;

    return caps;
}


////////////////////////////////////////////////////////////
Joystick::Identification JoystickImpl::getIdentification() const
{
    return m_identification;
}


////////////////////////////////////////////////////////////
JoystickState JoystickImpl::update()
{
    JoystickState state;

    // Get the current joystick state
    JOYINFOEX pos;
    pos.dwFlags  = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV | JOY_RETURNBUTTONS;
    pos.dwFlags |= (m_caps.wCaps & JOYCAPS_POVCTS) ? JOY_RETURNPOVCTS : JOY_RETURNPOV;
    pos.dwSize   = sizeof(JOYINFOEX);
    if (joyGetPosEx(m_index, &pos) == JOYERR_NOERROR)
    {
        // The joystick is connected
        state.connected = true;

        // Axes
        state.axes[Joystick::X] = (pos.dwXpos - (m_caps.wXmax + m_caps.wXmin) / 2.f) * 200.f / (m_caps.wXmax - m_caps.wXmin);
        state.axes[Joystick::Y] = (pos.dwYpos - (m_caps.wYmax + m_caps.wYmin) / 2.f) * 200.f / (m_caps.wYmax - m_caps.wYmin);
        state.axes[Joystick::Z] = (pos.dwZpos - (m_caps.wZmax + m_caps.wZmin) / 2.f) * 200.f / (m_caps.wZmax - m_caps.wZmin);
        state.axes[Joystick::R] = (pos.dwRpos - (m_caps.wRmax + m_caps.wRmin) / 2.f) * 200.f / (m_caps.wRmax - m_caps.wRmin);
        state.axes[Joystick::U] = (pos.dwUpos - (m_caps.wUmax + m_caps.wUmin) / 2.f) * 200.f / (m_caps.wUmax - m_caps.wUmin);
        state.axes[Joystick::V] = (pos.dwVpos - (m_caps.wVmax + m_caps.wVmin) / 2.f) * 200.f / (m_caps.wVmax - m_caps.wVmin);

        // Special case for POV, it is given as an angle
        if (pos.dwPOV != 0xFFFF)
        {
            float angle = pos.dwPOV / 18000.f * 3.141592654f;
            state.axes[Joystick::PovX] = std::sin(angle) * 100;
            state.axes[Joystick::PovY] = std::cos(angle) * 100;
        }
        else
        {
            state.axes[Joystick::PovX] = 0;
            state.axes[Joystick::PovY] = 0;
        }

        // Buttons
        for (unsigned int i = 0; i < Joystick::ButtonCount; ++i)
            state.buttons[i] = (pos.dwButtons & (1 << i)) != 0;
    }

    return state;
}

} // namespace priv

} // namespace sf
