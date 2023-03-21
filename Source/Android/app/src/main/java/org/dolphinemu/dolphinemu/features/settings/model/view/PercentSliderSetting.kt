// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import kotlin.math.roundToInt

class PercentSliderSetting(
    context: Context,
    override val setting: AbstractFloatSetting,
    titleId: Int,
    descriptionId: Int,
    min: Int,
    max: Int,
    units: String?,
    stepSize: Int
) : FloatSliderSetting(context, setting, titleId, descriptionId, min, max, units, stepSize) {
    override val selectedValue: Int
        get() = (floatSetting.float * 100).roundToInt()

    override fun setSelectedValue(settings: Settings?, selection: Float) {
        floatSetting.setFloat(settings!!, selection / 100)
    }
}
