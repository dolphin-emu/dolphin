// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context

sealed class SliderSetting : SettingsItem {
    override val type: Int = TYPE_SLIDER

    var min: Any
        private set
    var max: Any
        private set
    var units: String?
        private set
    var stepSize: Any = 0
        private set
    var showDecimal: Boolean = false
        private set

    constructor(
        context: Context,
        nameId: Int,
        descriptionId: Int,
        min: Any,
        max: Any,
        units: String?,
        stepSize: Any,
        showDecimal: Boolean
    ) : super(context, nameId, descriptionId) {
        this.min = min
        this.max = max
        this.units = units
        this.stepSize = stepSize
        this.showDecimal = showDecimal
    }

    constructor(
        name: CharSequence,
        description: CharSequence?,
        min: Any,
        max: Any,
        units: String?,
        stepSize: Any,
        showDecimal: Boolean
    ) : super(name, description) {
        this.min = min
        this.max = max
        this.units = units
        this.stepSize = stepSize
        this.showDecimal = showDecimal
    }

    abstract val selectedValue: Any
}
