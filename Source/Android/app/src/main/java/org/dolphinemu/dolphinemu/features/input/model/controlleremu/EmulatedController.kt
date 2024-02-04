// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu

import androidx.annotation.Keep

/**
 * Represents a C++ ControllerEmu::EmulatedController.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
@Keep
class EmulatedController private constructor(private val pointer: Long) {
    external fun getDefaultDevice(): String

    external fun setDefaultDevice(device: String)

    external fun getGroupCount(): Int

    external fun getGroup(index: Int): ControlGroup

    external fun updateSingleControlReference(controlReference: ControlReference)

    external fun loadDefaultSettings()

    external fun clearSettings()

    external fun loadProfile(path: String)

    external fun saveProfile(path: String)

    external fun getProfileKey(): String

    external fun getUserProfileDirectoryPath(): String

    external fun getSysProfileDirectoryPath(): String

    companion object {
        @JvmStatic
        external fun getGcPad(controllerIndex: Int): EmulatedController

        @JvmStatic
        external fun getGcKeyboard(controllerIndex: Int): EmulatedController

        @JvmStatic
        external fun getWiimote(controllerIndex: Int): EmulatedController

        @JvmStatic
        external fun getWiimoteAttachment(
            controllerIndex: Int,
            attachmentIndex: Int
        ): EmulatedController

        @JvmStatic
        external fun getSelectedWiimoteAttachment(controllerIndex: Int): Int

        @JvmStatic
        external fun getSidewaysWiimoteSetting(controllerIndex: Int): NumericSetting
    }
}
