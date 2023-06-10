// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu

import androidx.annotation.Keep

/**
 * Represents a C++ ControllerEmu::ControlGroup.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
@Keep
class ControlGroup private constructor(private val pointer: Long) {
    external fun getUiName(): String

    external fun getGroupType(): Int

    external fun getDefaultEnabledValue(): Int

    external fun getEnabled(): Boolean

    external fun setEnabled(value: Boolean)

    external fun getControlCount(): Int

    external fun getControl(i: Int): Control

    external fun getNumericSettingCount(): Int

    external fun getNumericSetting(i: Int): NumericSetting

    /**
     * If getGroupType returns TYPE_ATTACHMENTS, this returns the attachment selection setting.
     * Otherwise, undefined behavior!
     */
    external fun getAttachmentSetting(): NumericSetting

    companion object {
        const val TYPE_OTHER = 0
        const val TYPE_STICK = 1
        const val TYPE_MIXED_TRIGGERS = 2
        const val TYPE_BUTTONS = 3
        const val TYPE_FORCE = 4
        const val TYPE_ATTACHMENTS = 5
        const val TYPE_TILT = 6
        const val TYPE_CURSOR = 7
        const val TYPE_TRIGGERS = 8
        const val TYPE_SLIDER = 9
        const val TYPE_SHAKE = 10
        const val TYPE_IMU_ACCELEROMETER = 11
        const val TYPE_IMU_GYROSCOPE = 12
        const val TYPE_IMU_CURSOR = 13

        const val DEFAULT_ENABLED_ALWAYS = 0
        const val DEFAULT_ENABLED_YES = 1
        const val DEFAULT_ENABLED_NO = 2
    }
}
