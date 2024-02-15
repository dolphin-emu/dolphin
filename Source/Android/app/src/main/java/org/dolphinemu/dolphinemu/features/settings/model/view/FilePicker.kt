// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class FilePicker(
    context: Context,
    override var setting: AbstractStringSetting,
    titleId: Int,
    descriptionId: Int,
    val requestType: Int,
    val defaultPathRelativeToUserDirectory: String?
) : SettingsItem(context, titleId, descriptionId) {
    override val type: Int = TYPE_FILE_PICKER

    fun getSelectedValue() : String {
        return setting.string
    }

    fun setSelectedValue(settings: Settings, selection: String) {
        setting.setString(settings, selection)
    }
}
