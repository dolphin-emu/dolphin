// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

open class SwitchSetting : SettingsItem {
    override val type: Int = TYPE_SWITCH

    protected var booleanSetting: AbstractBooleanSetting

    override val setting: AbstractSetting
        get() = booleanSetting

    constructor(
        context: Context,
        setting: AbstractBooleanSetting,
        titleId: Int,
        descriptionId: Int
    ) : super(context, titleId, descriptionId) {
        booleanSetting = setting
    }

    constructor(
        setting: AbstractBooleanSetting,
        title: CharSequence?,
        description: CharSequence?
    ) : super(title!!, description) {
        booleanSetting = setting
    }

    open val isChecked: Boolean
        get() = booleanSetting.boolean

    open fun setChecked(settings: Settings?, checked: Boolean) {
        booleanSetting.setBoolean(settings!!, checked)
    }
}
