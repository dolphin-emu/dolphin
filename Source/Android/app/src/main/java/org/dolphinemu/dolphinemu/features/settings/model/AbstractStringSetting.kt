// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

interface AbstractStringSetting : AbstractSetting {
    val string: String

    fun setString(settings: Settings, newValue: String)
}
