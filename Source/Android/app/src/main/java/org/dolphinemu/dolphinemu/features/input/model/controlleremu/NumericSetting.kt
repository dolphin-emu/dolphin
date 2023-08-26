// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu

import androidx.annotation.Keep

/**
 * Represents a C++ ControllerEmu::NumericSetting.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
@Keep
class NumericSetting private constructor(private val pointer: Long) {
    /**
     * @return The name used in the UI.
     */
    external fun getUiName(): String

    /**
     * @return A string applied to the number in the UI (unit of measure).
     */
    external fun getUiSuffix(): String

    /**
     * @return Detailed description of the setting.
     */
    external fun getUiDescription(): String

    /**
     * @return TYPE_INT, TYPE_DOUBLE or TYPE_BOOLEAN
     */
    external fun getType(): Int

    external fun getControlReference(): ControlReference

    /**
     * If the type is TYPE_INT, gets the current value. Otherwise, undefined behavior!
     */
    external fun getIntValue(): Int

    /**
     * If the type is TYPE_INT, sets the current value. Otherwise, undefined behavior!
     */
    external fun setIntValue(value: Int)

    /**
     * If the type is TYPE_INT, gets the default value. Otherwise, undefined behavior!
     */
    external fun getIntDefaultValue(): Int

    /**
     * If the type is TYPE_DOUBLE, gets the current value. Otherwise, undefined behavior!
     */
    external fun getDoubleValue(): Double

    /**
     * If the type is TYPE_DOUBLE, sets the current value. Otherwise, undefined behavior!
     */
    external fun setDoubleValue(value: Double)

    /**
     * If the type is TYPE_DOUBLE, gets the default value. Otherwise, undefined behavior!
     */
    external fun getDoubleDefaultValue(): Double

    /**
     * If the type is TYPE_DOUBLE, returns the minimum valid value. Otherwise, undefined behavior!
     */
    external fun getDoubleMin(): Double

    /**
     * If the type is TYPE_DOUBLE, returns the maximum valid value. Otherwise, undefined behavior!
     */
    external fun getDoubleMax(): Double

    /**
     * If the type is TYPE_BOOLEAN, gets the current value. Otherwise, undefined behavior!
     */
    external fun getBooleanValue(): Boolean

    /**
     * If the type is TYPE_BOOLEAN, sets the current value. Otherwise, undefined behavior!
     */
    external fun setBooleanValue(value: Boolean)

    /**
     * If the type is TYPE_BOOLEAN, gets the default value. Otherwise, undefined behavior!
     */
    external fun getBooleanDefaultValue(): Boolean

    companion object {
        const val TYPE_INT = 0
        const val TYPE_DOUBLE = 1
        const val TYPE_BOOLEAN = 2
    }
}
