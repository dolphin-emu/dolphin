// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import java.io.File

object GbaLinkSettings {
    const val SIDEVICE_GC_GBA_EMULATED = 13

    const val SCALE_MODE_FIT = 0
    const val SCALE_MODE_STRETCH = 1
    const val SCALE_MODE_INTEGER = 2

    const val POSITION_CENTER = 0
    const val POSITION_TOP = 1
    const val POSITION_BOTTOM = 2
    const val POSITION_LEFT = 3
    const val POSITION_RIGHT = 4

    fun normalizeScaleMode(scaleMode: Int): Int = when (scaleMode) {
        SCALE_MODE_FIT,
        SCALE_MODE_STRETCH,
        SCALE_MODE_INTEGER -> scaleMode
        else -> SCALE_MODE_FIT
    }

    fun normalizePosition(position: Int): Int = when (position) {
        POSITION_CENTER,
        POSITION_TOP,
        POSITION_BOTTOM,
        POSITION_LEFT,
        POSITION_RIGHT -> position
        else -> POSITION_CENTER
    }

    fun hasAnyEmulatedGbaPortConfigured(): Boolean =
        siDeviceSettings.any { it.int == SIDEVICE_GC_GBA_EMULATED }

    fun shouldWarnAboutMissingBios(): Boolean =
        siDeviceSettings.indices.any { index ->
            siDeviceSettings[index].int == SIDEVICE_GC_GBA_EMULATED &&
                    gbaRomSettings[index].string.isBlank()
        } && !hasUsableBios()

    fun hasConfiguredPhysicalController(deviceNumber: Int): Boolean {
        val defaultDevice = getGbaPhysicalDevice(deviceNumber) ?: return false
        return ControllerInterface.getDevice(defaultDevice) != null
    }

    fun hasIndependentPhysicalController(deviceNumber: Int): Boolean =
        getGbaPhysicalDevice(deviceNumber) != null &&
                getGameCubePortsSharingPhysicalController(deviceNumber).isEmpty()

    fun getGameCubePortsSharingPhysicalController(deviceNumber: Int): IntArray {
        val gbaDevice = getGbaPhysicalDevice(deviceNumber) ?: return IntArray(0)
        return GBA_DEVICE_RANGE.filter { controllerIndex ->
            siDeviceSettings[controllerIndex].int in GAMECUBE_PAD_DEVICE_TYPES &&
                    getPhysicalDevice(EmulatedController.getGcPad(controllerIndex)) == gbaDevice
        }.toIntArray()
    }

    private fun getGbaPhysicalDevice(deviceNumber: Int): String? {
        if (deviceNumber !in GBA_DEVICE_RANGE) {
            return null
        }

        return getPhysicalDevice(EmulatedController.getGbaPad(deviceNumber))
    }

    private fun getPhysicalDevice(controller: EmulatedController): String? {
        val defaultDevice = controller.getDefaultDevice()
        return defaultDevice.takeIf {
            it.isNotEmpty() && !it.endsWith("/Touchscreen")
        }
    }

    private val GBA_DEVICE_RANGE = 0..3
    private val GAMECUBE_PAD_DEVICE_TYPES = intArrayOf(
        SIDEVICE_GC_CONTROLLER,
        SIDEVICE_GC_STEERING,
        SIDEVICE_DANCEMAT,
        SIDEVICE_GC_TARUKONGA,
        SIDEVICE_AM_BASEBOARD
    )

    private val siDeviceSettings = arrayOf(
        IntSetting.MAIN_SI_DEVICE_0,
        IntSetting.MAIN_SI_DEVICE_1,
        IntSetting.MAIN_SI_DEVICE_2,
        IntSetting.MAIN_SI_DEVICE_3
    )

    private val gbaRomSettings = arrayOf(
        StringSetting.MAIN_GBA_ROM_1,
        StringSetting.MAIN_GBA_ROM_2,
        StringSetting.MAIN_GBA_ROM_3,
        StringSetting.MAIN_GBA_ROM_4
    )

    private fun hasUsableBios(): Boolean {
        val configuredPath = StringSetting.MAIN_GBA_BIOS_PATH.string
        val path = configuredPath.ifBlank {
            File(DirectoryInitialization.getUserDirectory(), "GBA/gba_bios.bin").path
        }

        val size = if (ContentHandler.isContentUri(path)) {
            ContentHandler.getSizeAndIsDirectory(path)
        } else {
            val file = File(path)
            if (file.isFile) file.length() else -1
        }

        return size == GBA_BIOS_SIZE
    }

    private const val GBA_BIOS_SIZE = 0x4000L
    private const val SIDEVICE_GC_CONTROLLER = 6
    private const val SIDEVICE_GC_STEERING = 8
    private const val SIDEVICE_DANCEMAT = 9
    private const val SIDEVICE_GC_TARUKONGA = 10
    private const val SIDEVICE_AM_BASEBOARD = 11
}
