// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlGroup
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class ControlGroupEnabledSetting(private val controlGroup: ControlGroup) : AbstractBooleanSetting {
    override val boolean: Boolean
        get() = controlGroup.getEnabled()

    override fun setBoolean(settings: Settings, newValue: Boolean) =
        controlGroup.setEnabled(newValue)

    override val isOverridden: Boolean = false

    override val isRuntimeEditable: Boolean = true

    override fun delete(settings: Settings): Boolean {
        val newValue = controlGroup.getDefaultEnabledValue() != ControlGroup.DEFAULT_ENABLED_NO
        controlGroup.setEnabled(newValue)
        return true
    }
}
