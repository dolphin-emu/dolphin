// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class DateTimeChoiceSetting(
    context: Context,
    override val setting: AbstractStringSetting,
    nameId: Int,
    descriptionId: Int
) : SettingsItem(context, nameId, descriptionId) {
    override val type: Int = TYPE_DATETIME_CHOICE

    fun setSelectedValue(settings: Settings, selection: String) {
        setting.setString(settings, selection)
    }

    fun getSelectedValue(): String {
        return setting.string
    }
}
