// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Build
import android.view.InputDevice
import android.view.Surface
import androidx.annotation.Keep
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.utils.Log
import java.util.Collections

class DolphinSensorEventListener : SensorEventListener {
    private class AxisSetDetails(val firstAxisOfSet: Int, val axisSetType: Int)

    private class SensorDetails(
        val sensor: Sensor,
        val sensorType: Int,
        val axisNames: Array<String>,
        val axisSetDetails: Array<AxisSetDetails>
    ) {
        var isSuspended = true
        var hasRegisteredListener = false
    }

    private val sensorManager: SensorManager?

    private val sensorDetails = ArrayList<SensorDetails>()

    private val rotateCoordinatesForScreenOrientation: Boolean

    /**
     * AOSP has a bug in InputDeviceSensorManager where
     * InputSensorEventListenerDelegate.removeSensor attempts to modify an ArrayList it's iterating
     * through in a way that throws a ConcurrentModificationException. Because of this, we can't
     * suspend individual sensors for InputDevices, but we can suspend all sensors at once.
     */
    private val canSuspendSensorsIndividually: Boolean

    private var unsuspendedSensors = 0

    private var deviceQualifier = ""

    @Keep
    constructor() {
        sensorManager = DolphinApplication.getAppContext()
            .getSystemService(Context.SENSOR_SERVICE) as SensorManager?
        rotateCoordinatesForScreenOrientation = true
        canSuspendSensorsIndividually = true
        addSensors()
        sortSensorDetails()
    }

    @Keep
    constructor(inputDevice: InputDevice) {
        rotateCoordinatesForScreenOrientation = false
        canSuspendSensorsIndividually = false
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            sensorManager = inputDevice.sensorManager
            addSensors()
        } else {
            sensorManager = null
        }
        sortSensorDetails()
    }

    private fun addSensors() {
        tryAddSensor(
            Sensor.TYPE_ACCELEROMETER,
            arrayOf(
                "Accel Right",
                "Accel Left",
                "Accel Forward",
                "Accel Backward",
                "Accel Up",
                "Accel Down"
            ),
            arrayOf(AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES))
        )

        tryAddSensor(
            Sensor.TYPE_GYROSCOPE,
            arrayOf(
                "Gyro Pitch Up",
                "Gyro Pitch Down",
                "Gyro Roll Right",
                "Gyro Roll Left",
                "Gyro Yaw Left",
                "Gyro Yaw Right"
            ),
            arrayOf(AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES))
        )

        tryAddSensor(Sensor.TYPE_LIGHT, "Light")

        tryAddSensor(Sensor.TYPE_PRESSURE, "Pressure")

        tryAddSensor(Sensor.TYPE_TEMPERATURE, "Device Temperature")

        tryAddSensor(Sensor.TYPE_PROXIMITY, "Proximity")

        tryAddSensor(
            Sensor.TYPE_GRAVITY,
            arrayOf(
                "Gravity Right",
                "Gravity Left",
                "Gravity Forward",
                "Gravity Backward",
                "Gravity Up",
                "Gravity Down"
            ),
            arrayOf(AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES))
        )

        tryAddSensor(
            Sensor.TYPE_LINEAR_ACCELERATION,
            arrayOf(
                "Linear Acceleration Right",
                "Linear Acceleration Left",
                "Linear Acceleration Forward",
                "Linear Acceleration Backward",
                "Linear Acceleration Up",
                "Linear Acceleration Down"
            ),
            arrayOf(AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES))
        )

        // The values provided by this sensor can be interpreted as an Euler vector or a quaternion.
        // The directions of X and Y are flipped to match the Wii Remote coordinate system.
        tryAddSensor(
            Sensor.TYPE_ROTATION_VECTOR,
            arrayOf(
                "Rotation Vector X-",
                "Rotation Vector X+",
                "Rotation Vector Y-",
                "Rotation Vector Y+",
                "Rotation Vector Z+",
                "Rotation Vector Z-",
                "Rotation Vector R",
                "Rotation Vector Heading Accuracy"
            ),
            arrayOf(AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES))
        )

        tryAddSensor(Sensor.TYPE_RELATIVE_HUMIDITY, "Relative Humidity")

        tryAddSensor(Sensor.TYPE_AMBIENT_TEMPERATURE, "Ambient Temperature")

        // The values provided by this sensor can be interpreted as an Euler vector or a quaternion.
        // The directions of X and Y are flipped to match the Wii Remote coordinate system.
        tryAddSensor(
            Sensor.TYPE_GAME_ROTATION_VECTOR,
            arrayOf(
                "Game Rotation Vector X-",
                "Game Rotation Vector X+",
                "Game Rotation Vector Y-",
                "Game Rotation Vector Y+",
                "Game Rotation Vector Z+",
                "Game Rotation Vector Z-",
                "Game Rotation Vector R"
            ),
            arrayOf(AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES))
        )

        tryAddSensor(
            Sensor.TYPE_GYROSCOPE_UNCALIBRATED,
            arrayOf(
                "Gyro Uncalibrated Pitch Up",
                "Gyro Uncalibrated Pitch Down",
                "Gyro Uncalibrated Roll Right",
                "Gyro Uncalibrated Roll Left",
                "Gyro Uncalibrated Yaw Left",
                "Gyro Uncalibrated Yaw Right",
                "Gyro Drift Pitch Up",
                "Gyro Drift Pitch Down",
                "Gyro Drift Roll Right",
                "Gyro Drift Roll Left",
                "Gyro Drift Yaw Left",
                "Gyro Drift Yaw Right"
            ),
            arrayOf(
                AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES),
                AxisSetDetails(3, AXIS_SET_TYPE_DEVICE_COORDINATES)
            )
        )

        tryAddSensor(Sensor.TYPE_HEART_RATE, "Heart Rate")

        if (Build.VERSION.SDK_INT >= 24) {
            tryAddSensor(Sensor.TYPE_HEART_BEAT, "Heart Beat")
        }

        if (Build.VERSION.SDK_INT >= 26) {
            tryAddSensor(
                Sensor.TYPE_ACCELEROMETER_UNCALIBRATED,
                arrayOf(
                    "Accel Uncalibrated Right",
                    "Accel Uncalibrated Left",
                    "Accel Uncalibrated Forward",
                    "Accel Uncalibrated Backward",
                    "Accel Uncalibrated Up",
                    "Accel Uncalibrated Down",
                    "Accel Bias Right",
                    "Accel Bias Left",
                    "Accel Bias Forward",
                    "Accel Bias Backward",
                    "Accel Bias Up",
                    "Accel Bias Down"
                ),
                arrayOf(
                    AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES),
                    AxisSetDetails(3, AXIS_SET_TYPE_DEVICE_COORDINATES)
                )
            )
        }

        if (Build.VERSION.SDK_INT >= 30) {
            tryAddSensor(Sensor.TYPE_HINGE_ANGLE, "Hinge Angle")
        }

        if (Build.VERSION.SDK_INT >= 33) {
            // The values provided by this sensor can be interpreted as an Euler vector.
            // The directions of X and Y are flipped to match the Wii Remote coordinate system.
            tryAddSensor(
                Sensor.TYPE_HEAD_TRACKER,
                arrayOf(
                    "Head Rotation Vector X-",
                    "Head Rotation Vector X+",
                    "Head Rotation Vector Y-",
                    "Head Rotation Vector Y+",
                    "Head Rotation Vector Z+",
                    "Head Rotation Vector Z-",
                    "Head Pitch Up",
                    "Head Pitch Down",
                    "Head Roll Right",
                    "Head Roll Left",
                    "Head Yaw Left",
                    "Head Yaw Right"
                ),
                arrayOf(
                    AxisSetDetails(0, AXIS_SET_TYPE_OTHER_COORDINATES),
                    AxisSetDetails(3, AXIS_SET_TYPE_OTHER_COORDINATES)
                )
            )

            tryAddSensor(Sensor.TYPE_HEADING, arrayOf("Heading", "Heading Accuracy"), arrayOf())
        }
    }

    private fun tryAddSensor(sensorType: Int, axisName: String) {
        tryAddSensor(sensorType, arrayOf(axisName), arrayOf())
    }

    private fun tryAddSensor(
        sensorType: Int,
        axisNames: Array<String>,
        axisSetDetails: Array<AxisSetDetails>
    ) {
        val sensor = sensorManager!!.getDefaultSensor(sensorType)
        if (sensor != null) {
            sensorDetails.add(SensorDetails(sensor, sensorType, axisNames, axisSetDetails))
        }
    }

    private fun sortSensorDetails() {
        Collections.sort(
            sensorDetails,
            Comparator.comparingInt { s: SensorDetails -> s.sensorType }
        )
    }

    override fun onSensorChanged(sensorEvent: SensorEvent) {
        val sensorDetails = sensorDetails.first{s -> sensorsAreEqual(s.sensor, sensorEvent.sensor)}

        val values = sensorEvent.values
        val axisNames = sensorDetails.axisNames
        val axisSetDetails = sensorDetails.axisSetDetails

        var eventAxisIndex = 0
        var detailsAxisIndex = 0
        var detailsAxisSetIndex = 0
        var keepSensorAlive = false
        while (eventAxisIndex < values.size && detailsAxisIndex < axisNames.size) {
            if (detailsAxisSetIndex < axisSetDetails.size &&
                axisSetDetails[detailsAxisSetIndex].firstAxisOfSet == eventAxisIndex
            ) {
                var rotation = Surface.ROTATION_0
                if (rotateCoordinatesForScreenOrientation &&
                    axisSetDetails[detailsAxisSetIndex].axisSetType == AXIS_SET_TYPE_DEVICE_COORDINATES
                ) {
                    rotation = deviceRotation
                }

                var x: Float
                var y: Float
                when (rotation) {
                    Surface.ROTATION_0 -> {
                        x = values[eventAxisIndex]
                        y = values[eventAxisIndex + 1]
                    }

                    Surface.ROTATION_90 -> {
                        x = -values[eventAxisIndex + 1]
                        y = values[eventAxisIndex]
                    }

                    Surface.ROTATION_180 -> {
                        x = -values[eventAxisIndex]
                        y = -values[eventAxisIndex + 1]
                    }

                    Surface.ROTATION_270 -> {
                        x = values[eventAxisIndex + 1]
                        y = -values[eventAxisIndex]
                    }

                    else -> {
                        x = values[eventAxisIndex]
                        y = values[eventAxisIndex + 1]
                    }
                }

                val z = values[eventAxisIndex + 2]

                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex],
                    x
                )
                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex + 1],
                    x
                )
                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex + 2],
                    y
                )
                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex + 3],
                    y
                )
                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex + 4],
                    z
                )
                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex + 5],
                    z
                )

                eventAxisIndex += 3
                detailsAxisIndex += 6
                detailsAxisSetIndex++
            } else {
                keepSensorAlive = keepSensorAlive or ControllerInterface.dispatchSensorEvent(
                    deviceQualifier,
                    axisNames[detailsAxisIndex], values[eventAxisIndex]
                )

                eventAxisIndex++
                detailsAxisIndex++
            }
        }
        if (!keepSensorAlive) {
            setSensorSuspended(sensorDetails, true)
        }
    }

    override fun onAccuracyChanged(sensor: Sensor, i: Int) {
        // We don't care about this
    }

    /**
     * The device qualifier set here will be passed on to native code,
     * for the purpose of letting native code identify which device this object belongs to.
     */
    @Keep
    fun setDeviceQualifier(deviceQualifier: String) {
        this.deviceQualifier = deviceQualifier
    }

    /**
     * If a sensor has been suspended to save battery, this unsuspends it.
     * If the sensor isn't currently suspended, nothing happens.
     *
     * @param axisName The name of any of the sensor's axes.
     */
    @Keep
    fun requestUnsuspendSensor(axisName: String) {
        for (sd in sensorDetails) {
            if (listOf(*sd.axisNames).contains(axisName)) {
                setSensorSuspended(sd, false)
            }
        }
    }

    private fun setSensorSuspended(sensorDetails: SensorDetails, suspend: Boolean) {
        var changeOccurred = false

        synchronized(sensorDetails) {
            if (sensorDetails.isSuspended != suspend) {
                ControllerInterface.notifySensorSuspendedState(
                    deviceQualifier,
                    sensorDetails.axisNames,
                    suspend
                )

                if (suspend) {
                    unsuspendedSensors -= 1
                } else {
                    unsuspendedSensors += 1
                }

                if (canSuspendSensorsIndividually) {
                    if (suspend) {
                        sensorManager!!.unregisterListener(this, sensorDetails.sensor)
                    } else {
                        sensorManager!!.registerListener(
                            this,
                            sensorDetails.sensor,
                            SAMPLING_PERIOD_US
                        )
                    }
                    sensorDetails.hasRegisteredListener = !suspend
                } else {
                    if (suspend) {
                        // If there are no unsuspended sensors left, unregister them all.
                        // Otherwise, leave unregistering for later. A possible alternative could be
                        // to unregister everything and then re-register the sensors we still want,
                        // but I fear this could lead to dropped inputs.
                        if (unsuspendedSensors == 0) {
                            sensorManager!!.unregisterListener(this)
                            for (sd in this.sensorDetails) {
                                sd.hasRegisteredListener = false
                            }
                        }
                    } else {
                        if (!sensorDetails.hasRegisteredListener) {
                            sensorManager!!.registerListener(
                                this,
                                sensorDetails.sensor,
                                SAMPLING_PERIOD_US
                            )
                            sensorDetails.hasRegisteredListener = true
                        }
                    }
                }

                sensorDetails.isSuspended = suspend

                changeOccurred = true
            }
        }

        if (changeOccurred) {
            Log.info((if (suspend) "Suspended sensor " else "Unsuspended sensor ") + sensorDetails.sensor.name)
        }
    }

    @Keep
    fun getAxisNames(): Array<String> {
        val axisNames = ArrayList<String>()
        for (sensorDetails in sensorDetails) {
            sensorDetails.axisNames.forEach { axisNames.add(it) }
        }
        return axisNames.toArray(arrayOf())
    }

    @Keep
    fun getNegativeAxes(): BooleanArray {
        val negativeAxes = ArrayList<Boolean>()

        for (sensorDetails in sensorDetails) {
            var eventAxisIndex = 0
            var detailsAxisIndex = 0
            var detailsAxisSetIndex = 0
            while (detailsAxisIndex < sensorDetails.axisNames.size) {
                if (detailsAxisSetIndex < sensorDetails.axisSetDetails.size &&
                    sensorDetails.axisSetDetails[detailsAxisSetIndex].firstAxisOfSet == eventAxisIndex
                ) {
                    negativeAxes.add(false)
                    negativeAxes.add(true)
                    negativeAxes.add(false)
                    negativeAxes.add(true)
                    negativeAxes.add(false)
                    negativeAxes.add(true)

                    eventAxisIndex += 3
                    detailsAxisIndex += 6
                    detailsAxisSetIndex++
                } else {
                    negativeAxes.add(false)

                    eventAxisIndex++
                    detailsAxisIndex++
                }
            }
        }

        val result = BooleanArray(negativeAxes.size)
        for (i in result.indices) {
            result[i] = negativeAxes[i]
        }

        return result
    }

    companion object {
        // Set of three axes. Creates a negative companion to each axis, and corrects for device rotation.
        private const val AXIS_SET_TYPE_DEVICE_COORDINATES = 0

        // Set of three axes. Creates a negative companion to each axis.
        private const val AXIS_SET_TYPE_OTHER_COORDINATES = 1

        private var deviceRotation = Surface.ROTATION_0

        // The fastest sampling rate Android lets us use without declaring the HIGH_SAMPLING_RATE_SENSORS
        // permission is 200 Hz. This is also the sampling rate of a Wii Remote, so it fits us perfectly.
        private const val SAMPLING_PERIOD_US = 1000000 / 200

        /**
         * Should be called when an activity or other component that uses sensor events is resumed.
         *
         * Sensor events that contain device coordinates will have the coordinates rotated by the value
         * passed to this function.
         *
         * @param deviceRotation The current rotation of the device (i.e. rotation of the default display)
         */
        fun setDeviceRotation(deviceRotation: Int) {
            this.deviceRotation = deviceRotation
        }

        private fun sensorsAreEqual(s1: Sensor, s2: Sensor): Boolean {
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && s1.id > 0 && s2.id > 0) {
                s1.type == s2.type && s1.id == s2.id
            } else {
                s1.type == s2.type && s1.name == s2.name
            }
        }
    }
}
