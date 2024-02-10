// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class SingleChoiceSettingDynamicDescriptions(
    context: Context,
    override val setting: AbstractIntSetting,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Int,
    val valuesId: Int,
    val descriptionChoicesId: Int,
    val descriptionValuesId: Int,
) : SettingsItem(context, titleId, descriptionId) {
    override val type: Int = TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS

    val selectedValue: Int
        get() = setting.int

    fun setSelectedValue(settings: Settings, selection: Int) {
        setting.setInt(settings, selection)
    }
}
