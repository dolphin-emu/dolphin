// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag

open class StringSingleChoiceSetting : SettingsItem {
    override val type: Int = TYPE_SINGLE_CHOICE

    private val stringSetting: AbstractStringSetting?

    override val setting: AbstractSetting?
        get() = stringSetting

    var choices: Array<String>
        protected set
    var values: Array<String>
        protected set
    val menuTag: MenuTag?
    var noChoicesAvailableString = 0
        private set

    open val selectedChoice: String
        get() = getChoiceAt(selectedValueIndex)

    open val selectedValue: String
        get() = stringSetting!!.string

    @JvmOverloads
    constructor(
        context: Context,
        setting: AbstractStringSetting?,
        titleId: Int,
        descriptionId: Int,
        choices: Array<String>,
        values: Array<String>,
        menuTag: MenuTag? = null
    ) : super(context, titleId, descriptionId) {
        stringSetting = setting
        this.choices = choices
        this.values = values
        this.menuTag = menuTag
    }

    constructor(
        context: Context,
        setting: AbstractStringSetting,
        titleId: Int,
        descriptionId: Int,
        choices: Array<String>,
        values: Array<String>,
        noChoicesAvailableString: Int
    ) : this(context, setting, titleId, descriptionId, choices, values) {
        this.noChoicesAvailableString = noChoicesAvailableString
    }

    @JvmOverloads
    constructor(
        context: Context,
        setting: AbstractStringSetting,
        titleId: Int,
        descriptionId: Int,
        choicesId: Int,
        valuesId: Int,
        menuTag: MenuTag? = null
    ) : super(context, titleId, descriptionId) {
        stringSetting = setting
        choices = DolphinApplication.getAppContext().resources.getStringArray(choicesId)
        values = DolphinApplication.getAppContext().resources.getStringArray(valuesId)
        this.menuTag = menuTag
    }

    fun getChoiceAt(index: Int): String {
        return if (index >= 0 && index < choices.size) {
            choices[index]
        } else ""
    }

    fun getValueAt(index: Int): String {
        return if (index >= 0 && index < values.size) {
            values[index]
        } else ""
    }

    val selectedValueIndex: Int
        get() {
            val selectedValue = selectedValue
            for (i in values.indices) {
                if (values[i] == selectedValue) {
                    return i
                }
            }
            return -1
        }

    open fun setSelectedValue(settings: Settings, selection: String) {
        stringSetting!!.setString(settings, selection)
    }

    open fun refreshChoicesAndValues() {}
}
