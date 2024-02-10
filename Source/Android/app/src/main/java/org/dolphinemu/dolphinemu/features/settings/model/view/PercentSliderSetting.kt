// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import java.math.BigDecimal
import java.math.MathContext

class PercentSliderSetting(
    context: Context,
    override val setting: AbstractFloatSetting,
    titleId: Int,
    descriptionId: Int,
    min: Float,
    max: Float,
    units: String,
    stepSize: Float,
    showDecimal: Boolean
) : FloatSliderSetting(
    context,
    setting,
    titleId,
    descriptionId,
    min,
    max,
    units,
    stepSize,
    showDecimal
) {
    override val selectedValue: Float
        get() = (floatSetting.float * 100)

    override fun setSelectedValue(settings: Settings, selection: Float) {
        floatSetting.setFloat(
            settings,
            BigDecimal((selection / 100).toDouble()).round(MathContext(3)).toFloat()
        )
    }
}
