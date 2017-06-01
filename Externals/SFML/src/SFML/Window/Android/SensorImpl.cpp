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
#include <SFML/Window/SensorImpl.hpp>
#include <SFML/System/Time.hpp>
#include <android/looper.h>

// Define missing constants
#define ASENSOR_TYPE_GRAVITY             0x00000009
#define ASENSOR_TYPE_LINEAR_ACCELERATION 0x0000000a
#define ASENSOR_TYPE_ORIENTATION         0x00000003

namespace
{
    ALooper* looper;
    ASensorManager*    sensorManager;
    ASensorEventQueue* sensorEventQueue;
    sf::Vector3f       sensorData[sf::Sensor::Count];
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
void SensorImpl::initialize()
{
    // Get the looper associated with this thread
    looper = ALooper_forThread();

    // Get the unique sensor manager
    sensorManager = ASensorManager_getInstance();

    // Create the sensor events queue and attach it to the looper
    sensorEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
        1, &processSensorEvents, NULL);
}


////////////////////////////////////////////////////////////
void SensorImpl::cleanup()
{
    // Detach the sensor events queue from the looper and destroy it
    ASensorManager_destroyEventQueue(sensorManager, sensorEventQueue);
}


////////////////////////////////////////////////////////////
bool SensorImpl::isAvailable(Sensor::Type sensor)
{
    const ASensor* available = getDefaultSensor(sensor);

    return available? true : false;
}


////////////////////////////////////////////////////////////
bool SensorImpl::open(Sensor::Type sensor)
{
    // Get the default sensor matching the type
    m_sensor = getDefaultSensor(sensor);

    // Sensor not available, stop here
    if (!m_sensor)
        return false;

    // Get the minimum delay allowed between events
    Time minimumDelay = microseconds(ASensor_getMinDelay(m_sensor));

    // Set the event rate (not to consume too much battery)
    ASensorEventQueue_setEventRate(sensorEventQueue, m_sensor, minimumDelay.asMicroseconds());

    // Save the index of the sensor
    m_index = static_cast<unsigned int>(sensor);

    return true;
}


////////////////////////////////////////////////////////////
void SensorImpl::close()
{
    // Nothing to do
}


////////////////////////////////////////////////////////////
Vector3f SensorImpl::update()
{
    // Update our sensor data list
    ALooper_pollAll(0, NULL, NULL, NULL);

    return sensorData[m_index];
}


////////////////////////////////////////////////////////////
void SensorImpl::setEnabled(bool enabled)
{
    if (enabled)
        ASensorEventQueue_enableSensor(sensorEventQueue, m_sensor);
    else
        ASensorEventQueue_disableSensor(sensorEventQueue, m_sensor);
}


////////////////////////////////////////////////////////////
ASensor const* SensorImpl::getDefaultSensor(Sensor::Type sensor)
{
    // Find the Android sensor type
    static int types[] = {ASENSOR_TYPE_ACCELEROMETER, ASENSOR_TYPE_GYROSCOPE,
        ASENSOR_TYPE_MAGNETIC_FIELD, ASENSOR_TYPE_GRAVITY, ASENSOR_TYPE_LINEAR_ACCELERATION,
        ASENSOR_TYPE_ORIENTATION};

    int type = types[sensor];

    // Retrieve the default sensor matching this type
    return ASensorManager_getDefaultSensor(sensorManager, type);
}


////////////////////////////////////////////////////////////
int SensorImpl::processSensorEvents(int fd, int events, void* data)
{
    ASensorEvent event;

    while (ASensorEventQueue_getEvents(sensorEventQueue, &event, 1) > 0)
    {
        unsigned int type = Sensor::Count;
        Vector3f data;

        switch (event.type)
        {
            case ASENSOR_TYPE_ACCELEROMETER:
                type = Sensor::Accelerometer;
                data.x = event.acceleration.x;
                data.y = event.acceleration.y;
                data.z = event.acceleration.z;
                break;

            case ASENSOR_TYPE_GYROSCOPE:
                type = Sensor::Gyroscope;
                data.x = event.vector.x;
                data.y = event.vector.y;
                data.z = event.vector.z;
                break;

            case ASENSOR_TYPE_MAGNETIC_FIELD:
                type = Sensor::Magnetometer;
                data.x = event.magnetic.x;
                data.y = event.magnetic.y;
                data.z = event.magnetic.z;
                break;

            case ASENSOR_TYPE_GRAVITY:
                type = Sensor::Gravity;
                data.x = event.vector.x;
                data.y = event.vector.y;
                data.z = event.vector.z;
                break;

            case ASENSOR_TYPE_LINEAR_ACCELERATION:
                type = Sensor::UserAcceleration;
                data.x = event.acceleration.x;
                data.y = event.acceleration.y;
                data.z = event.acceleration.z;
                break;

            case ASENSOR_TYPE_ORIENTATION:
                type = Sensor::Orientation;
                data.x = event.vector.x;
                data.y = event.vector.y;
                data.z = event.vector.z;
                break;
        }

        // An unknown sensor event has been detected, we don't know how to process it
        if (type == Sensor::Count)
            continue;

        sensorData[type] = data;
    }

    return 1;
}

} // namespace priv

} // namespace sf
