// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting

open class HeaderSetting : SettingsItem {
    override val setting: AbstractSetting? = null

    constructor(
        context: Context,
        titleId: Int,
        descriptionId: Int
    ) : super(context, titleId, descriptionId)

    constructor(title: CharSequence, description: CharSequence?) : super(title, description)

    override val type: Int = TYPE_HEADER
}
