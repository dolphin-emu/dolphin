// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

interface AbstractIntSetting : AbstractSetting {
    val int: Int

    fun setInt(settings: Settings, newValue: Int)
}
