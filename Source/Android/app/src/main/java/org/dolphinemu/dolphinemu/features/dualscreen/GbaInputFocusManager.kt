// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import org.dolphinemu.dolphinemu.features.input.model.InputOverrider

object GbaInputFocusManager {
    private var focusedDevice = GbaHostBridge.NO_DEVICE
    private var blockedGameCubePorts = IntArray(0)

    fun onGbaInputRegistered(deviceNumber: Int) {
        if (deviceNumber in GBA_DEVICE_RANGE &&
            GbaLinkSettings.hasIndependentPhysicalController(deviceNumber)
        ) {
            InputOverrider.setGbaInputEnabled(deviceNumber, true)
        }
    }

    fun requestFocus(deviceNumber: Int) {
        if (deviceNumber !in GBA_DEVICE_RANGE) {
            return
        }

        if (focusedDevice != deviceNumber) {
            clearFocus()
            focusedDevice = deviceNumber
            blockedGameCubePorts =
                GbaLinkSettings.getGameCubePortsSharingPhysicalController(deviceNumber)
        }

        setGameCubeInputEnabled(blockedGameCubePorts, false)
        InputOverrider.setGbaInputEnabled(deviceNumber, true)
    }

    fun clearFocus(deviceNumber: Int? = null) {
        val currentDevice = focusedDevice
        if (currentDevice !in GBA_DEVICE_RANGE ||
            deviceNumber != null && deviceNumber != currentDevice
        ) {
            return
        }

        InputOverrider.setGbaInputEnabled(
            currentDevice,
            GbaLinkSettings.hasIndependentPhysicalController(currentDevice)
        )
        setGameCubeInputEnabled(blockedGameCubePorts, true)
        blockedGameCubePorts = IntArray(0)
        focusedDevice = GbaHostBridge.NO_DEVICE
    }

    private fun setGameCubeInputEnabled(controllerIndices: IntArray, enabled: Boolean) {
        for (controllerIndex in controllerIndices) {
            InputOverrider.registerGameCube(controllerIndex)
            InputOverrider.setGameCubeInputEnabled(controllerIndex, enabled)
        }
    }

    private val GBA_DEVICE_RANGE = 0..3
}
