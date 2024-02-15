// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class InputStringSetting(
    context: Context,
    setting: AbstractStringSetting,
    titleId: Int,
    descriptionId: Int,
) : SettingsItem(context, titleId, descriptionId) {
    override val type: Int = TYPE_STRING

    private var stringSetting: AbstractStringSetting = setting

    override val setting: AbstractSetting
        get() = stringSetting

    val selectedValue: String
        get() = stringSetting.string

    fun setSelectedValue(settings: Settings, selection: String) {
        stringSetting.setString(settings, selection)
    }
}
