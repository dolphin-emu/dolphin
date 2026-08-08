// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import android.os.Handler
import android.os.Looper
import androidx.annotation.Keep
import java.util.concurrent.CopyOnWriteArraySet

object GbaHostBridge {
    private data class Frame(
        val deviceNumber: Int,
        val width: Int,
        val height: Int,
        val pixels: IntArray
    )

    data class CoreInfo(
        val deviceNumber: Int,
        val width: Int,
        val height: Int,
        val isGba: Boolean,
        val hasRom: Boolean,
        val title: String
    )

    interface Listener {
        fun onGbaGameChanged(info: CoreInfo) {}
        fun onGbaVisibleDeviceChanged(deviceNumber: Int, info: CoreInfo?) {}
        fun onGbaFrame(deviceNumber: Int, width: Int, height: Int, pixels: IntArray) {}
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val infos = arrayOfNulls<CoreInfo>(DEVICE_COUNT)

    private val listeners = CopyOnWriteArraySet<Listener>()
    private val frameLock = Any()
    private var pendingFrame: Frame? = null
    private var frameDispatchPosted = false
    private var frameConsumerCount = 0

    @Volatile
    var visibleDeviceNumber: Int = NO_DEVICE
        private set

    @JvmStatic
    external fun setVisibleDevice(deviceNumber: Int)

    fun addListener(listener: Listener) {
        listeners.add(listener)
        listener.onGbaVisibleDeviceChanged(visibleDeviceNumber, getVisibleInfo())
    }

    fun removeListener(listener: Listener) {
        listeners.remove(listener)
    }

    fun registerFrameConsumer() {
        frameConsumerCount++
        refreshVisibleDevice()
    }

    fun unregisterFrameConsumer() {
        frameConsumerCount--
        refreshVisibleDevice()
    }

    fun hasActiveCore(): Boolean =
        infos.any { it?.isGba == true || it?.hasRom == true }

    private fun getVisibleInfo(): CoreInfo? =
        if (visibleDeviceNumber in infos.indices) infos[visibleDeviceNumber] else null

    private fun refreshVisibleDevice() {
        val nextDevice = when {
            frameConsumerCount == 0 -> NO_DEVICE
            infos[GB_PLAYER_DEVICE]?.hasRom == true -> GB_PLAYER_DEVICE
            else -> LINK_DEVICE_RANGE.firstOrNull { infos[it]?.hasRom == true }
                ?: LINK_DEVICE_RANGE.firstOrNull { infos[it]?.isGba == true }
                ?: NO_DEVICE
        }

        if (nextDevice != visibleDeviceNumber) {
            visibleDeviceNumber = nextDevice
            setVisibleDevice(nextDevice)
            val info = getVisibleInfo()
            for (listener in listeners) {
                listener.onGbaVisibleDeviceChanged(nextDevice, info)
            }
        }
    }

    @Keep
    @JvmStatic
    fun onGameChanged(
        deviceNumber: Int,
        width: Int,
        height: Int,
        isGba: Boolean,
        hasRom: Boolean,
        title: String
    ) {
        mainHandler.post {
            val info = CoreInfo(deviceNumber, width, height, isGba, hasRom, title)
            infos[deviceNumber] = info
            refreshVisibleDevice()
            for (listener in listeners) {
                listener.onGbaGameChanged(info)
            }
        }
    }

    @Keep
    @JvmStatic
    fun onCoreStopped(deviceNumber: Int) {
        mainHandler.post {
            infos[deviceNumber] = null
            refreshVisibleDevice()
        }
    }

    @Keep
    @JvmStatic
    fun onFrame(deviceNumber: Int, width: Int, height: Int, pixels: IntArray) {
        if (deviceNumber != visibleDeviceNumber) {
            return
        }

        synchronized(frameLock) {
            pendingFrame = Frame(deviceNumber, width, height, pixels)
            if (frameDispatchPosted) {
                return
            }
            frameDispatchPosted = true
        }
        mainHandler.post(::dispatchPendingFrame)
    }

    private fun dispatchPendingFrame() {
        val frame = synchronized(frameLock) {
            frameDispatchPosted = false
            pendingFrame.also { pendingFrame = null }
        } ?: return

        if (frame.deviceNumber == visibleDeviceNumber) {
            for (listener in listeners) {
                listener.onGbaFrame(
                    frame.deviceNumber,
                    frame.width,
                    frame.height,
                    frame.pixels
                )
            }
        }
    }

    const val NO_DEVICE = -1
    const val GB_PLAYER_DEVICE = 4

    private const val DEVICE_COUNT = 5
    private val LINK_DEVICE_RANGE = 0..3
}
