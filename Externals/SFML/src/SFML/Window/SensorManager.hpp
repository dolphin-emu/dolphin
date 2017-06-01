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

#ifndef SFML_SENSORMANAGER_HPP
#define SFML_SENSORMANAGER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Sensor.hpp>
#include <SFML/Window/SensorImpl.hpp>
#include <SFML/System/NonCopyable.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Global sensor manager
///
////////////////////////////////////////////////////////////
class SensorManager : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Get the global unique instance of the manager
    ///
    /// \return Unique instance of the sensor manager
    ///
    ////////////////////////////////////////////////////////////
    static SensorManager& getInstance();

    ////////////////////////////////////////////////////////////
    /// \brief Check if a sensor is available on the underlying platform
    ///
    /// \param sensor Sensor to check
    ///
    /// \return True if the sensor is available, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    bool isAvailable(Sensor::Type sensor);

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable a sensor
    ///
    /// \param sensor  Sensor to modify
    /// \param enabled Whether it should be enabled or not
    ///
    ////////////////////////////////////////////////////////////
    void setEnabled(Sensor::Type sensor, bool enabled);

    ////////////////////////////////////////////////////////////
    /// \brief Check if a sensor is enabled
    ///
    /// \param sensor Sensor to check
    ///
    /// \return True if the sensor is enabled, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    bool isEnabled(Sensor::Type sensor) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the current value of a sensor
    ///
    /// \param sensor Sensor to read
    ///
    /// \return Current value of the sensor
    ///
    ////////////////////////////////////////////////////////////
    Vector3f getValue(Sensor::Type sensor) const;

    ////////////////////////////////////////////////////////////
    /// \brief Update the state of all the sensors
    ///
    ////////////////////////////////////////////////////////////
    void update();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    SensorManager();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~SensorManager();

    ////////////////////////////////////////////////////////////
    /// \brief Sensor information and state
    ///
    ////////////////////////////////////////////////////////////
    struct Item
    {
        bool available;    ///< Is the sensor available on this device?
        bool enabled;      ///< Current enable state of the sensor
        SensorImpl sensor; ///< Sensor implementation
        Vector3f value;    ///< The current sensor value
    };

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Item m_sensors[Sensor::Count]; ///< Sensors information and state
};

} // namespace priv

} // namespace sf


#endif // SFML_SENSORMANAGER_HPP
