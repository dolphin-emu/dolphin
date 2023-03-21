// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag

class SubmenuSetting(
    context: Context,
    titleId: Int,
    val menuKey: MenuTag
) : SettingsItem(context, titleId, 0) {
    override val type: Int = TYPE_SUBMENU

    override val setting: AbstractSetting? = null
}
