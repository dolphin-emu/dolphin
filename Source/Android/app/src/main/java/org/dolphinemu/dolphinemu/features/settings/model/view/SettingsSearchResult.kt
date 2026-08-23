// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.os.Bundle
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag

class SettingsSearchResult(
    name: CharSequence,
    description: CharSequence,
    val menuKey: MenuTag,
    val settingPosition: Int,
    val navigationExtras: Bundle?
) : SettingsItem(name, description) {
    override val type: Int = TYPE_SEARCH_RESULT

    override val setting: AbstractSetting? = null
}
