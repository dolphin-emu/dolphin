// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context

abstract class SliderSetting : SettingsItem {
    override val type: Int = TYPE_SLIDER

    var min: Int
        private set
    var max: Int
        private set
    var units: String?
        private set
    var stepSize = 0
        private set

    constructor(
        context: Context,
        nameId: Int,
        descriptionId: Int,
        min: Int,
        max: Int,
        units: String?,
        stepSize: Int
    ) : super(context, nameId, descriptionId) {
        this.min = min
        this.max = max
        this.units = units
        this.stepSize = stepSize
    }

    constructor(
        name: CharSequence,
        description: CharSequence?,
        min: Int,
        max: Int,
        units: String?
    ) : super(name, description) {
        this.min = min
        this.max = max
        this.units = units
    }

    abstract val selectedValue: Int
}
