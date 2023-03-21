// SPDX-License-Identifier: GPL-2.0-or-later
package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag

class SingleChoiceSetting(
    context: Context,
    private val intSetting: AbstractIntSetting,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Int,
    val valuesId: Int,
    val menuTag: MenuTag? = null
) : SettingsItem(context, titleId, descriptionId) {
    override val type: Int = TYPE_SINGLE_CHOICE

    override val setting: AbstractSetting
        get() = intSetting

    val selectedValue: Int
        get() = intSetting.int

    fun setSelectedValue(settings: Settings?, selection: Int) {
        intSetting.setInt(settings!!, selection)
    }
}
