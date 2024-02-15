// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import java.math.BigDecimal
import java.math.MathContext

open class FloatSliderSetting : SliderSetting {
    protected val floatSetting: AbstractFloatSetting

    val min: Float
    val max: Float
    val stepSize: Float

    override val setting: AbstractSetting
        get() = floatSetting

    constructor(
        context: Context,
        setting: AbstractFloatSetting,
        titleId: Int,
        descriptionId: Int,
        min: Float,
        max: Float,
        units: String,
        stepSize: Float,
        showDecimal: Boolean
    ) : super(context, titleId, descriptionId, units, showDecimal) {
        floatSetting = setting
        this.min = min
        this.max = max
        this.stepSize = stepSize
    }

    constructor(
        setting: AbstractFloatSetting,
        name: CharSequence,
        description: CharSequence,
        min: Float,
        max: Float,
        units: String,
        stepSize: Float,
        showDecimal: Boolean
    ) : super(name, description, units, showDecimal) {
        floatSetting = setting
        this.min = min
        this.max = max
        this.stepSize = stepSize
    }

    open val selectedValue: Float
        get() = floatSetting.float

    open fun setSelectedValue(settings: Settings, selection: Float) {
        floatSetting.setFloat(settings, selection)
    }
}
