// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

interface AbstractBooleanSetting : AbstractSetting {
    val boolean: Boolean

    fun setBoolean(settings: Settings, newValue: Boolean)
}
