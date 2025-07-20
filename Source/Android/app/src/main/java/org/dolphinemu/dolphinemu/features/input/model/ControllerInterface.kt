// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import android.content.Context
import android.hardware.input.InputManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.annotation.Keep
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.utils.LooperThread
import java.util.concurrent.atomic.AtomicBoolean

/**
 * This class interfaces with the native ControllerInterface,
 * which is where the emulator core gets inputs from.
 */
object ControllerInterface {
    private var inputDeviceListener: InputDeviceListener? = null
    private lateinit var looperThread: LooperThread

    private var inputStateUpdatePending = AtomicBoolean(false)
    private val inputStateVersion = MutableLiveData(0)
    private val devicesVersion = MutableLiveData(0)

    val inputStateChanged: LiveData<Int>
        get() = inputStateVersion

    val devicesChanged: LiveData<Int>
        get() = devicesVersion

    /**
     * Activities which want to pass on inputs to native code
     * should call this in their own dispatchKeyEvent method.
     *
     * @return true if the emulator core seems to be interested in this event.
     * false if the event should be passed on to the default dispatchKeyEvent.
     */
    fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val result = dispatchKeyEventNative(event)
        onInputStateChanged()
        return result
    }

    private external fun dispatchKeyEventNative(event: KeyEvent): Boolean

    /**
     * Activities which want to pass on inputs to native code
     * should call this in their own dispatchGenericMotionEvent method.
     *
     * @return true if the emulator core seems to be interested in this event.
     * false if the event should be passed on to the default dispatchGenericMotionEvent.
     */
    fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        val result = dispatchGenericMotionEventNative(event)
        onInputStateChanged()
        return result
    }

    private external fun dispatchGenericMotionEventNative(event: MotionEvent): Boolean

    /**
     * [DolphinSensorEventListener] calls this for each axis of a received SensorEvent.
     *
     * @return true if the emulator core seems to be interested in this event.
     * false if the sensor can be suspended to save battery.
     */
    fun dispatchSensorEvent(deviceQualifier: String, axisName: String, value: Float): Boolean {
        val result = dispatchSensorEventNative(deviceQualifier, axisName, value)
        onInputStateChanged()
        return result
    }

    private external fun dispatchSensorEventNative(
        deviceQualifier: String,
        axisName: String,
        value: Float
    ): Boolean

    /**
     * Called when a sensor is suspended or unsuspended.
     *
     * @param deviceQualifier A string used by native code for uniquely identifying devices.
     * @param axisNames       The name of all axes for the sensor.
     * @param suspended       Whether the sensor is now suspended.
     */
    external fun notifySensorSuspendedState(
        deviceQualifier: String,
        axisNames: Array<String>,
        suspended: Boolean
    )

    /**
     * Rescans for input devices.
     */
    external fun refreshDevices()

    external fun getAllDeviceStrings(): Array<String>

    external fun getDevice(deviceString: String): CoreDevice?

    private fun onInputStateChanged() {
        // When a single SensorEvent is dispatched, this method is likely to get called many times.
        // For the sake of performance, let's batch input state updates so that observers only have
        // to process one update.
        if (!inputStateUpdatePending.getAndSet(true)) {
            Handler(Looper.getMainLooper()).post {
                if (inputStateUpdatePending.getAndSet(false)) {
                    inputStateVersion.value = inputStateVersion.value?.plus(1)
                }
            }
        }
    }

    @Keep
    @JvmStatic
    private fun onDevicesChanged() {
        Handler(Looper.getMainLooper()).post {
            devicesVersion.value = devicesVersion.value?.plus(1)
        }
    }

    @Keep
    @JvmStatic
    private fun registerInputDeviceListener() {
        looperThread = LooperThread("Hotplug thread")
        looperThread.start()

        if (inputDeviceListener == null) {
            val im = DolphinApplication.getAppContext()
                .getSystemService(Context.INPUT_SERVICE) as InputManager?

            inputDeviceListener = InputDeviceListener()
            im!!.registerInputDeviceListener(inputDeviceListener, Handler(looperThread.looper))
        }
    }

    @Keep
    @JvmStatic
    private fun unregisterInputDeviceListener() {
        if (inputDeviceListener != null) {
            val im = DolphinApplication.getAppContext()
                .getSystemService(Context.INPUT_SERVICE) as InputManager?

            im!!.unregisterInputDeviceListener(inputDeviceListener)
            inputDeviceListener = null
        }
    }

    @Keep
    @JvmStatic
    private fun getVibratorManager(device: InputDevice): DolphinVibratorManager {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            DolphinVibratorManagerPassthrough(device.vibratorManager)
        } else {
            DolphinVibratorManagerCompat(device.vibrator)
        }
    }

    @Keep
    @JvmStatic
    private fun getSystemVibratorManager(): DolphinVibratorManager {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vibratorManager = DolphinApplication.getAppContext()
                .getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager?
            if (vibratorManager != null)
                return DolphinVibratorManagerPassthrough(vibratorManager)
        }
        val vibrator = DolphinApplication.getAppContext()
            .getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
        return DolphinVibratorManagerCompat(vibrator)
    }

    @Keep
    @JvmStatic
    private fun vibrate(vibrator: Vibrator) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(100, VibrationEffect.DEFAULT_AMPLITUDE))
        } else {
            vibrator.vibrate(100)
        }
    }

    private class InputDeviceListener : InputManager.InputDeviceListener {
        // Simple implementation for now. We could do something fancier if we wanted to.
        override fun onInputDeviceAdded(deviceId: Int) = refreshDevices()

        // Simple implementation for now. We could do something fancier if we wanted to.
        override fun onInputDeviceRemoved(deviceId: Int) = refreshDevices()

        // Simple implementation for now. We could do something fancier if we wanted to.
        override fun onInputDeviceChanged(deviceId: Int) = refreshDevices()
    }
}
