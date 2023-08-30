// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import java.math.BigDecimal
import java.math.MathContext

open class FloatSliderSetting : SliderSetting {
    var floatSetting: AbstractFloatSetting

    override val setting: AbstractSetting
        get() = floatSetting

    constructor(
        context: Context,
        setting: AbstractFloatSetting,
        titleId: Int,
        descriptionId: Int,
        min: Float,
        max: Float,
        units: String?,
        stepSize: Float,
        showDecimal: Boolean
    ) : super(context, titleId, descriptionId, min, max, units, stepSize, showDecimal) {
        floatSetting = setting
    }

    constructor(
        setting: AbstractFloatSetting,
        name: CharSequence,
        description: CharSequence?,
        min: Float,
        max: Float,
        units: String?,
        showDecimal: Boolean
    ) : super(name, description, min, max, units, showDecimal) {
        floatSetting = setting
    }

    override val selectedValue: Float
        get() = floatSetting.float

    open fun setSelectedValue(settings: Settings?, selection: Float) {
        floatSetting.setFloat(
            settings!!,
            BigDecimal((selection).toDouble()).round(MathContext(3)).toFloat()
        )
    }
}
