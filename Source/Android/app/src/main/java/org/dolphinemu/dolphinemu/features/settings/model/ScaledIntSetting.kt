// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

class ScaledIntSetting(
    private val scale: Int,
    private val setting: AbstractIntSetting
) : AbstractIntSetting {
    override fun isOverridden(): Boolean {
        return setting.isOverridden()
    }

    override fun isRuntimeEditable(): Boolean {
        return setting.isRuntimeEditable
    }

    override fun delete(settings: Settings): Boolean {
        return setting.delete(settings)
    }

    override fun getInt(): Int {
        return setting.getInt() / scale
    }

    override fun setInt(settings: Settings, newValue: Int) {
        return setting.setInt(settings, newValue * scale)
    }
}
