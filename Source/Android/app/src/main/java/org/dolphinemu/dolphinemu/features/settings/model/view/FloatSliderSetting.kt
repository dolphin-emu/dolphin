// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import kotlin.math.roundToInt

open class FloatSliderSetting : SliderSetting {
    var floatSetting: AbstractFloatSetting

    override val setting: AbstractSetting
        get() = floatSetting

    constructor(
        context: Context,
        setting: AbstractFloatSetting,
        titleId: Int,
        descriptionId: Int,
        min: Int,
        max: Int,
        units: String?,
        stepSize: Int
    ) : super(context, titleId, descriptionId, min, max, units, stepSize) {
        floatSetting = setting
    }

    constructor(
        setting: AbstractFloatSetting,
        name: CharSequence,
        description: CharSequence?,
        min: Int,
        max: Int,
        units: String?
    ) : super(name, description, min, max, units) {
        floatSetting = setting
    }

    override val selectedValue: Int
        get() = floatSetting.float.roundToInt()

    open fun setSelectedValue(settings: Settings?, selection: Float) {
        floatSetting.setFloat(settings!!, selection)
    }
}
