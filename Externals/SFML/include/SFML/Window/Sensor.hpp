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

#ifndef SFML_SENSOR_HPP
#define SFML_SENSOR_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Export.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/System/Time.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Give access to the real-time state of the sensors
///
////////////////////////////////////////////////////////////
class SFML_WINDOW_API Sensor
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Sensor type
    ///
    ////////////////////////////////////////////////////////////
    enum Type
    {
        Accelerometer,    ///< Measures the raw acceleration (m/s^2)
        Gyroscope,        ///< Measures the raw rotation rates (degrees/s)
        Magnetometer,     ///< Measures the ambient magnetic field (micro-teslas)
        Gravity,          ///< Measures the direction and intensity of gravity, independent of device acceleration (m/s^2)
        UserAcceleration, ///< Measures the direction and intensity of device acceleration, independent of the gravity (m/s^2)
        Orientation,      ///< Measures the absolute 3D orientation (degrees)

        Count             ///< Keep last -- the total number of sensor types
    };

    ////////////////////////////////////////////////////////////
    /// \brief Check if a sensor is available on the underlying platform
    ///
    /// \param sensor Sensor to check
    ///
    /// \return True if the sensor is available, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isAvailable(Type sensor);

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable a sensor
    ///
    /// All sensors are disabled by default, to avoid consuming too
    /// much battery power. Once a sensor is enabled, it starts
    /// sending events of the corresponding type.
    ///
    /// This function does nothing if the sensor is unavailable.
    ///
    /// \param sensor  Sensor to enable
    /// \param enabled True to enable, false to disable
    ///
    ////////////////////////////////////////////////////////////
    static void setEnabled(Type sensor, bool enabled);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current sensor value
    ///
    /// \param sensor Sensor to read
    ///
    /// \return The current sensor value
    ///
    ////////////////////////////////////////////////////////////
    static Vector3f getValue(Type sensor);
};

} // namespace sf


#endif // SFML_SENSOR_HPP


////////////////////////////////////////////////////////////
/// \class sf::Sensor
/// \ingroup window
///
/// sf::Sensor provides an interface to the state of the
/// various sensors that a device provides. It only contains static
/// functions, so it's not meant to be instantiated.
///
/// This class allows users to query the sensors values at any
/// time and directly, without having to deal with a window and
/// its events. Compared to the SensorChanged event, sf::Sensor
/// can retrieve the state of a sensor at any time (you don't need to
/// store and update its current value on your side).
///
/// Depending on the OS and hardware of the device (phone, tablet, ...),
/// some sensor types may not be available. You should always check
/// the availability of a sensor before trying to read it, with the
/// sf::Sensor::isAvailable function.
///
/// You may wonder why some sensor types look so similar, for example
/// Accelerometer and Gravity / UserAcceleration. The first one
/// is the raw measurement of the acceleration, and takes into account
/// both the earth gravity and the user movement. The others are
/// more precise: they provide these components separately, which is
/// usually more useful. In fact they are not direct sensors, they
/// are computed internally based on the raw acceleration and other sensors.
/// This is exactly the same for Gyroscope vs Orientation.
///
/// Because sensors consume a non-negligible amount of current, they are
/// all disabled by default. You must call sf::Sensor::setEnabled for each
/// sensor in which you are interested.
///
/// Usage example:
/// \code
/// if (sf::Sensor::isAvailable(sf::Sensor::Gravity))
/// {
///     // gravity sensor is available
/// }
///
/// // enable the gravity sensor
/// sf::Sensor::setEnabled(sf::Sensor::Gravity, true);
///
/// // get the current value of gravity
/// sf::Vector3f gravity = sf::Sensor::getValue(sf::Sensor::Gravity);
/// \endcode
///
////////////////////////////////////////////////////////////
