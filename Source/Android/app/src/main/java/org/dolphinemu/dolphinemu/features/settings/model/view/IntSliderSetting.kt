// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class IntSliderSetting(
    context: Context,
    private val intSetting: AbstractIntSetting,
    titleId: Int,
    descriptionId: Int,
    min: Int,
    max: Int,
    units: String?,
    stepSize: Int
) : SliderSetting(context, titleId, descriptionId, min, max, units, stepSize) {
    override val setting: AbstractSetting
        get() = intSetting

    override val selectedValue: Int
        get() = intSetting.int

    fun setSelectedValue(settings: Settings?, selection: Int) {
        intSetting.setInt(settings!!, selection)
    }
}
