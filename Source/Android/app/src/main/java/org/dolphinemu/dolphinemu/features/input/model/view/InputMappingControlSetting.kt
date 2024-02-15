// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.view

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.Control
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem

class InputMappingControlSetting(var control: Control, val controller: EmulatedController) :
    SettingsItem(control.getUiName(), "") {
    val controlReference get() = control.getControlReference()

    var value: String
        get() = controlReference.getExpression()
        set(expr) {
            controlReference.setExpression(expr)
            controller.updateSingleControlReference(controlReference)
        }

    fun clearValue() {
        value = ""
    }

    override val type: Int = TYPE_INPUT_MAPPING_CONTROL

    override val setting: AbstractSetting? = null

    override val isEditable: Boolean = true

    val isInput: Boolean
        get() = controlReference.isInput()
}
