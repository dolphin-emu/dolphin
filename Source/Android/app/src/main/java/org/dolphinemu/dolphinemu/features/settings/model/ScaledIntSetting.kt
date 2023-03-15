// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

class ScaledIntSetting(
    private val scale: Int,
    private val setting: AbstractIntSetting
) : AbstractIntSetting {
    override val isOverridden: Boolean
        get() = setting.isOverridden

    override val isRuntimeEditable: Boolean
        get() = setting.isRuntimeEditable

    override fun delete(settings: Settings): Boolean {
        return setting.delete(settings)
    }

    override val int: Int
        get() = setting.int / scale

    override fun setInt(settings: Settings, newValue: Int) {
        return setting.setInt(settings, newValue * scale)
    }
}
