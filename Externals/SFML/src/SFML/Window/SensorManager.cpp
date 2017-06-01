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
#include <SFML/Window/SensorManager.hpp>
#include <SFML/System/Err.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
SensorManager& SensorManager::getInstance()
{
    static SensorManager instance;
    return instance;
}


////////////////////////////////////////////////////////////
bool SensorManager::isAvailable(Sensor::Type sensor)
{
    return m_sensors[sensor].available;
}


////////////////////////////////////////////////////////////
void SensorManager::setEnabled(Sensor::Type sensor, bool enabled)
{
    if (m_sensors[sensor].available)
    {
        m_sensors[sensor].enabled = enabled;
        m_sensors[sensor].sensor.setEnabled(enabled);
    }
    else
    {
        err() << "Warning: trying to enable a sensor that is not available (call Sensor::isAvailable to check it)" << std::endl;
    }
}


////////////////////////////////////////////////////////////
bool SensorManager::isEnabled(Sensor::Type sensor) const
{
    return m_sensors[sensor].enabled;
}


////////////////////////////////////////////////////////////
Vector3f SensorManager::getValue(Sensor::Type sensor) const
{
    return m_sensors[sensor].value;
}


////////////////////////////////////////////////////////////
void SensorManager::update()
{
    for (int i = 0; i < Sensor::Count; ++i)
    {
        // Only process available sensors
        if (m_sensors[i].available)
            m_sensors[i].value = m_sensors[i].sensor.update();
    }
}


////////////////////////////////////////////////////////////
SensorManager::SensorManager()
{
    // Global sensor initialization
    SensorImpl::initialize();

    // Per sensor initialization
    for (int i = 0; i < Sensor::Count; ++i)
    {
        // Check which sensors are available
        m_sensors[i].available = SensorImpl::isAvailable(static_cast<Sensor::Type>(i));

        // Open the available sensors
        if (m_sensors[i].available)
        {
            m_sensors[i].sensor.open(static_cast<Sensor::Type>(i));
            m_sensors[i].sensor.setEnabled(false);
        }
    }
}

////////////////////////////////////////////////////////////
SensorManager::~SensorManager()
{
    // Per sensor cleanup
    for (int i = 0; i < Sensor::Count; ++i)
    {
        if (m_sensors[i].available)
            m_sensors[i].sensor.close();
    }

    // Global sensor cleanup
    SensorImpl::cleanup();
}

} // namespace priv

} // namespace sf
