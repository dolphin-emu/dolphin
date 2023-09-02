// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class InvertedSwitchSetting(
    context: Context,
    setting: AbstractBooleanSetting,
    titleId: Int,
    descriptionId: Int
) : SwitchSetting(context, setting, titleId, descriptionId) {
    override val setting: AbstractSetting
        get() = booleanSetting

    override val isChecked: Boolean
        get() = !booleanSetting.boolean

    override fun setChecked(settings: Settings, checked: Boolean) {
        booleanSetting.setBoolean(settings, !checked)
    }
}
