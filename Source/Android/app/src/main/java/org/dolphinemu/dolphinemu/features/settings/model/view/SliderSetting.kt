// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context

sealed class SliderSetting : SettingsItem {
    override val type: Int = TYPE_SLIDER

    val units: String
    val showDecimal: Boolean

    constructor(
        context: Context,
        nameId: Int,
        descriptionId: Int,
        units: String,
        showDecimal: Boolean
    ) : super(context, nameId, descriptionId) {
        this.units = units
        this.showDecimal = showDecimal
    }

    constructor(
        name: CharSequence,
        description: CharSequence,
        units: String,
        showDecimal: Boolean
    ) : super(name, description) {
        this.units = units
        this.showDecimal = showDecimal
    }
}
