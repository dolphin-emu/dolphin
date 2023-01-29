// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

class ScaledIntSetting(
    private val scale: Int,
    private val setting: AbstractIntSetting
) : AbstractIntSetting {
    override fun isOverridden(settings: Settings): Boolean {
        return setting.isOverridden(settings)
    }

    override fun isRuntimeEditable(): Boolean {
        return setting.isRuntimeEditable
    }

    override fun delete(settings: Settings): Boolean {
        return setting.delete(settings)
    }

    override fun getInt(settings: Settings): Int {
        return setting.getInt(settings) / scale
    }

    override fun setInt(settings: Settings, newValue: Int) {
        return setting.setInt(settings, newValue * scale)
    }
}
