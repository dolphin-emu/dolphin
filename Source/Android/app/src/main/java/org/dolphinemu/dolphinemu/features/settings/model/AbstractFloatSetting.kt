// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

interface AbstractFloatSetting : AbstractSetting {
    val float: Float

    fun setFloat(settings: Settings, newValue: Float)
}
