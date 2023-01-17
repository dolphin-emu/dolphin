// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class DateTimeChoiceSetting(
    context: Context,
    private val setting: AbstractStringSetting,
    nameId: Int,
    descriptionId: Int
) : SettingsItem(context, nameId, descriptionId) {
    override fun getType(): Int {
        return TYPE_DATETIME_CHOICE
    }

    override fun getSetting(): AbstractSetting {
        return setting
    }

    fun setSelectedValue(settings: Settings?, selection: String) {
        setting.setString(settings, selection)
    }

    fun getSelectedValue(settings: Settings): String {
        return setting.getString(settings)
    }
}
